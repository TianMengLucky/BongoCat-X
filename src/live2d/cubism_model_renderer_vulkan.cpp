#include "cubism_plugin_services.hpp"
#include "cubism_runtime.hpp"
#include <Rendering/Vulkan/CubismClass_Vulkan.hpp>
#include "cubism_vulkan_memory.hpp"
#include "cubism_vulkan_recording.hpp"
#include "cubism_vulkan_bindless.hpp"
#include <exception>
#include <cstring>
#include <stdexcept>

namespace bongo_cat {
uint32_t configure_vulkan_optimization(BongoCatModelRuntime *runtime,
    uint32_t requested, uint32_t backend) {
    if (!runtime || runtime->model) return 0;
    const auto &info = runtime->rhi_info;
    const bool vulkan = backend == BONGO_CAT_RHI_VULKAN && info.backend == BONGO_CAT_RHI_VULKAN;
    const bool allocation = vulkan && (requested & BONGO_CAT_OPTIMIZE_LARGE_ALLOCATION) != 0;
    uint32_t active = 0;
    if (bongo_cat::vulkan_memory_configure((VkInstance)info.vulkan_instance,
            (VkPhysicalDevice)info.vulkan_physical_device,
            (VkDevice)info.vulkan_device, allocation) && allocation)
        active |= BONGO_CAT_OPTIMIZE_LARGE_ALLOCATION;
    if (vulkan_recording_configure((VkDevice)info.vulkan_device, info.queue_family,
            vulkan && (requested & BONGO_CAT_OPTIMIZE_PARALLEL_RECORDING) != 0))
        active |= BONGO_CAT_OPTIMIZE_PARALLEL_RECORDING;
    if (vulkan_bindless_configure((VkDevice)info.vulkan_device,
            (VkPhysicalDevice)info.vulkan_physical_device,
            vulkan && (requested & BONGO_CAT_OPTIMIZE_BINDLESS) != 0 &&
            (runtime->vulkan_features & BONGO_CAT_VULKAN_SAMPLED_IMAGE_DYNAMIC_INDEXING) != 0))
        active |= BONGO_CAT_OPTIMIZE_BINDLESS;
    return active;
}

void release_vulkan_device() {
    class RendererAccess : public Csm::Rendering::CubismRenderer_Vulkan {
    public:
        using CubismRenderer_Vulkan::DoStaticRelease;
    };
    vulkan_recording_release();
    vulkan_bindless_release();
    RendererAccess::DoStaticRelease();
    vulkan_memory_release();
}

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
    std::vector<VkDescriptorImageInfo> atlases;
    for (const auto &texture : vulkan_textures_)
        atlases.push_back({texture.GetSampler(), texture.GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    vulkan_bindless_model(atlases.data(), atlases.size(),
        _model && !_model->IsBlendModeEnabled() && _model->GetOffscreenCount() == 0);
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
    const unsigned char *pixels, int width, int height, BongoCatError *error) {
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
        VkBuffer pooled = VK_NULL_HANDLE;
        VkBuffer handle() const { return pooled ? pooled : buffer.GetBuffer(); }
        ~Staging() {
            if (pooled) vulkan_buffer_destroy(pooled);
            else buffer.Destroy(device);
        }
    } staging{{}, device};
    try {
        const VkDeviceSize size = (VkDeviceSize)width * (VkDeviceSize)height * 4;
        const VkMemoryPropertyFlags memory_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        // This synchronous upload is the only transient buffer opted out of pooling.
        VkDeviceMemory memory = VK_NULL_HANDLE;
        if (vulkan_buffer_create(device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                memory_flags, &staging.pooled, &memory, true)) {
            void *mapped = nullptr;
            if (!vulkan_buffer_map(staging.pooled, &mapped) || !mapped)
                throw std::runtime_error("Cannot map Vulkan texture upload buffer");
            std::memcpy(mapped, pixels, (size_t)size);
            vulkan_buffer_unmap(staging.pooled);
        } else {
            staging.buffer.CreateBuffer(device, physical, size,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT, memory_flags);
            staging.buffer.Map(device, size);
            staging.buffer.MemCpy(pixels, size);
            staging.buffer.UnMap(device);
        }
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
        vkCmdCopyBufferToImage(command, staging.handle(), image.GetImage(),
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
    try { renderer->DrawModel(); }
    catch (...) { vulkan_recording_cancel(); throw; }
    native_visual_state(projection);
}
} // namespace bongo_cat
