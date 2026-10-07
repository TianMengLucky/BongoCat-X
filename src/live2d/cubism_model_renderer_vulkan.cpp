#include "cubism_model.hpp"
#include <Rendering/Vulkan/CubismClass_Vulkan.hpp>
#include <exception>
#include <stdexcept>

namespace bongo_cat {
Csm::Rendering::CubismRenderer *create_vulkan_renderer(
    Csm::csmUint32 width, Csm::csmUint32 height) {
    class Renderer final : public Csm::Rendering::CubismRenderer_Vulkan {
    public:
        Renderer(Csm::csmUint32 w, Csm::csmUint32 h) : CubismRenderer_Vulkan(w, h) {}
    };
    return CSM_NEW Renderer(width, height);
}

bool NativeModel::create_renderer_vulkan(BongoCatError *error) {
    const auto &info = rhi_info_;
    if (!info.vulkan_device || !info.image_count || !info.swapchain_views) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_CUBISM, "No Live2D Vulkan device attached");
        return false;
    }
    Csm::Rendering::CubismRenderer_Vulkan::SetConstantSettings(
        (VkDevice)info.vulkan_device, (VkPhysicalDevice)info.vulkan_physical_device,
        (VkCommandPool)info.vulkan_command_pool, (VkQueue)info.vulkan_queue,
        info.image_count, VkExtent2D{info.extent_width, info.extent_height},
        VK_NULL_HANDLE, (VkFormat)info.color_format, (VkFormat)info.depth_format);
    CreateRenderer((Csm::csmUint32)width_, (Csm::csmUint32)height_, prepare_mask_layout());
    auto *renderer = GetRenderer<Csm::Rendering::CubismRenderer_Vulkan>();
    if (!renderer) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_CUBISM, "Cannot create Live2D Vulkan renderer");
        return false;
    }
    for (auto &texture : vulkan_textures_) renderer->BindTexture(texture);
    renderer->IsPremultipliedAlpha(true);
    renderer_width_ = width_; renderer_height_ = height_;
    return true;
}

bool NativeModel::upload_texture_vulkan(int index,
    const std::vector<unsigned char> &pixels, int width, int height, BongoCatError *error) {
    VkDevice device = (VkDevice)rhi_info_.vulkan_device;
    VkPhysicalDevice physical = (VkPhysicalDevice)rhi_info_.vulkan_physical_device;
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physical, &properties);
    if ((uint32_t)width > properties.limits.maxImageDimension2D ||
        (uint32_t)height > properties.limits.maxImageDimension2D) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM, "Live2D texture exceeds Vulkan size limit");
        return false;
    }
    struct Staging {
        Csm::CubismBufferVulkan buffer;
        VkDevice device;
        ~Staging() { buffer.Destroy(device); }
    } staging{{}, device};
    try {
        staging.buffer.CreateBuffer(device, physical, (VkDeviceSize)pixels.size(),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        staging.buffer.Map(device, (VkDeviceSize)pixels.size());
        staging.buffer.MemCpy(pixels.data(), pixels.size());
        staging.buffer.UnMap(device);
        auto &image = vulkan_textures_[(size_t)index];
        image.CreateImage(device, physical, width, height, 1, VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        image.CreateView(device, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1);
        // No anisotropy feature is required for the one-level linear sampler.
        image.CreateSampler(device, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST, 0.0f, 1);
        auto *rhi = static_cast<const BongoCatRhi *>(rhi_info_.rhi_handle);
        VkCommandBuffer command = (VkCommandBuffer)bongo_cat_rhi_begin_commands(rhi);
        if (!command) throw std::runtime_error("Cannot allocate Vulkan upload commands");
        image.SetImageLayout(command, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, VK_IMAGE_ASPECT_COLOR_BIT);
        VkBufferImageCopy region{};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = VkExtent3D{(uint32_t)width, (uint32_t)height, 1};
        vkCmdCopyBufferToImage(command, staging.buffer.GetBuffer(), image.GetImage(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        image.SetImageLayout(command, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, VK_IMAGE_ASPECT_COLOR_BIT);
        if (!bongo_cat_rhi_submit_commands_checked(rhi, command))
            throw std::runtime_error("Cannot submit Vulkan texture upload");
        return true;
    } catch (const std::exception &exception) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_CUBISM, "%s", exception.what());
        return false;
    }
}

void NativeModel::release_textures_vulkan() {
    if (vulkan_textures_.empty()) return;
    VkDevice device = (VkDevice)rhi_info_.vulkan_device;
    for (auto &texture : vulkan_textures_) texture.Destroy(device);
    vulkan_textures_.clear();
}

void NativeModel::draw_vulkan() {
    auto *renderer = GetRenderer<Csm::Rendering::CubismRenderer_Vulkan>();
    if (!_model || !renderer || width_ <= 0 || height_ <= 0) return;
    BongoCatRhiVulkanFrameInfo frame{};
    if (!bongo_cat_rhi_get_vulkan_frame_info(
        static_cast<const BongoCatRhi *>(rhi_info_.rhi_handle), &frame)) return;
    Csm::Rendering::CubismRenderer_Vulkan::SetRenderTarget(
        (VkImage)frame.image, (VkImageView)frame.view,
        (VkFormat)rhi_info_.color_format, VkExtent2D{rhi_info_.extent_width, rhi_info_.extent_height});
    Csm::CubismMatrix44 projection;
    native_projection(projection);
    renderer->SetMvpMatrix(&projection);
    renderer->SetModelColor(1.0f, 1.0f, 1.0f, _model->GetModelOpacity());
    renderer->DrawModel();
    native_visual_state(projection);
}
} // namespace bongo_cat
