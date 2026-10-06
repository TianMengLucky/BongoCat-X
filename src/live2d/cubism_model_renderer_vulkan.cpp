/* Cubism Vulkan renderer paths of the Live2D bridge (compiled with
   BONGO_CAT_HAS_CUBISM_VULKAN; see docs/live2d-vulkan-metal.md). The
   renderer clears the target on its first draw of a frame and submits its
   own command buffers; the RHI's present path hands it the acquired frame
   through the frame hook set by the runtime. Texture upload follows the
   official Vulkan sample (staging buffer, layout transitions, mipmaps). */
#include "cubism_model.hpp"

#include "bongo_cat/safe_ffi.h"

#include <Rendering/Vulkan/CubismClass_Vulkan.hpp>

#include <SDL3/SDL_log.h>

#include <cmath>
#include <cstring>
#include <vector>

extern BongoCatRhiBackend s_live2d_rhi_backend;
namespace bongo_cat {

#ifdef BONGO_CAT_HAS_CUBISM_VULKAN


namespace {

/* Decodes a texture file to premultiplied RGBA8 through the memory-safe
   Rust decoder; the GL path re-encodes through its own pipeline, the Vulkan
   path uploads at the source resolution (UVs are normalized). */
std::vector<unsigned char> load_premultiplied_rgba(
    std::vector<unsigned char> file, int *width, int *height) {
    *width = *height = 0;
    std::vector<unsigned char> pixels;
    unsigned char *decoded = bongo_safe_image_decode(file.data(), file.size(),
        width, height);
    if (!decoded) return pixels;
    pixels.resize((size_t)*width * *height * 4);
    std::memcpy(pixels.data(), decoded, pixels.size());
    bongo_safe_free_pixels(decoded, (size_t)*width * *height);
    for (size_t i = 0; i + 3 < pixels.size(); i += 4) {
        unsigned alpha = pixels[i + 3];
        pixels[i + 0] = (unsigned char)(pixels[i + 0] * alpha / 255);
        pixels[i + 1] = (unsigned char)(pixels[i + 1] * alpha / 255);
        pixels[i + 2] = (unsigned char)(pixels[i + 2] * alpha / 255);
    }
    return pixels;
}

void copy_buffer_to_image(VkCommandBuffer command,
    VkBuffer buffer, VkImage image, int width, int height) {
    VkBufferImageCopy region = {};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = VkExtent3D{(uint32_t)width, (uint32_t)height, 1};
    vkCmdCopyBufferToImage(command, buffer, image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

} // namespace

void NativeModel::set_rhi_info(const BongoCatRhiDeviceInfo &info) {
    rhi_info_ = info;
}

/* Installs the renderer the generated CubismUserModel copy asks for. */
Csm::Rendering::CubismRenderer *bongo_cat_cubism_create_renderer(
    Csm::csmUint32 width, Csm::csmUint32 height) {
    if (s_live2d_rhi_backend != BONGO_CAT_RHI_VULKAN)
        return Csm::Rendering::CubismRenderer::Create(width, height);
    class VulkanBridgeRenderer final :
        public Csm::Rendering::CubismRenderer_Vulkan {
    public:
        explicit VulkanBridgeRenderer(Csm::csmUint32 w, Csm::csmUint32 h)
            : CubismRenderer_Vulkan(w, h) {}
    };
    return new VulkanBridgeRenderer(width, height);
}

const BongoCatRhiDeviceInfo &NativeModel::rhi_info() const {
    return rhi_info_;
}

bool NativeModel::create_renderer_vulkan(BongoCatError *error) {
    const BongoCatRhiDeviceInfo &info = rhi_info();
    if (!info.vulkan_device || !info.swapchain_views) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_CUBISM,
            "The Live2D Vulkan renderer has no device attached");
        return false;
    }
    Csm::Rendering::CubismRenderer_Vulkan::SetConstantSettings(
        (VkDevice)info.vulkan_device,
        (VkPhysicalDevice)info.vulkan_physical_device,
        (VkCommandPool)info.vulkan_command_pool,
        (VkQueue)info.vulkan_queue,
        info.image_count,
        VkExtent2D{info.extent_width, info.extent_height},
        (VkImageView)info.current_view,
        (VkFormat)info.color_format, (VkFormat)info.depth_format);
    const int buffer_count = prepare_mask_layout();
    /* The generated CubismUserModel copy routes the renderer installation
       through bongo_cat_cubism_create_renderer (below), which builds the
       Vulkan renderer for VK sessions and defers to the canonical OpenGL
       factory otherwise. */
    CreateRenderer((Csm::csmUint32)width_, (Csm::csmUint32)height_,
        buffer_count);
    auto *renderer = GetRenderer<Csm::Rendering::CubismRenderer_Vulkan>();
    if (!renderer) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_CUBISM,
            "Cannot create the Live2D Vulkan renderer");
        release_renderer();
        return false;
    }
    bind_textures_vulkan();
    renderer_width_ = width_;
    renderer_height_ = height_;
    return true;
}

void NativeModel::bind_textures_vulkan() {
    auto *renderer = GetRenderer<Csm::Rendering::CubismRenderer_Vulkan>();
    if (!renderer) return;
    const BongoCatRhiDeviceInfo &info = rhi_info();
    auto *rhi = (const BongoCatRhi *)info.rhi_handle;
    if (!rhi) return;
    int texture_count = setting_->GetTextureCount();
    for (int index = 0; index < texture_count; ++index) {
        int width = 0, height = 0;
        std::vector<unsigned char> pixels = load_premultiplied_rgba(
            read(path(setting_->GetTextureFileName(index))),
            &width, &height);
        if (pixels.empty() || width <= 0 || height <= 0) continue;
        VkDeviceSize size = (VkDeviceSize)pixels.size();
        Csm::CubismBufferVulkan staging;
        staging.CreateBuffer((VkDevice)info.vulkan_device,
            (VkPhysicalDevice)info.vulkan_physical_device, size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        staging.Map((VkDevice)info.vulkan_device, size);
        staging.MemCpy(pixels.data(), (size_t)size);
        staging.UnMap((VkDevice)info.vulkan_device);
        Csm::CubismImageVulkan image;
        image.CreateImage((VkDevice)info.vulkan_device,
            (VkPhysicalDevice)info.vulkan_physical_device,
            width, height, 1,
            VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT);
        VkCommandBuffer command =
            (VkCommandBuffer)bongo_cat_rhi_begin_commands(rhi);
        if (command) {
            image.SetImageLayout(command,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                VK_IMAGE_ASPECT_COLOR_BIT);
            copy_buffer_to_image(command, staging.GetBuffer(),
                image.GetImage(), width, height);
            image.SetImageLayout(command,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                VK_IMAGE_ASPECT_COLOR_BIT);
            bongo_cat_rhi_submit_commands(rhi, command);
        }
        image.CreateView((VkDevice)info.vulkan_device,
            VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1);
        image.CreateSampler((VkDevice)info.vulkan_device,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FILTER_LINEAR,
            VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST, 1.0f, 1);
        renderer->BindTexture(image);
        staging.Destroy((VkDevice)info.vulkan_device);
    }
    renderer->IsPremultipliedAlpha(true);
}

void NativeModel::draw_vulkan() {
    auto *renderer = GetRenderer<Csm::Rendering::CubismRenderer_Vulkan>();
    if (!_model || !renderer || width_ <= 0 || height_ <= 0) return;
    const BongoCatRhiDeviceInfo &info = rhi_info();
    if (!info.current_image) return;
    /* Per-frame target: the RHI refreshes rhi_info_ with the acquired
       swapchain image before this call (frame hook). */
    Csm::Rendering::CubismRenderer_Vulkan::SetRenderTarget(
        (VkImage)info.current_image, (VkImageView)info.current_view,
        (VkFormat)info.color_format,
        VkExtent2D{info.extent_width, info.extent_height});
    Csm::CubismMatrix44 projection;
    build_projection(projection,
        viewport_width_, viewport_height_);
    if (vertical_flip_) {
        float *matrix = projection.GetArray();
        for (int i = 1; i < 16; i += 4) matrix[i] = -matrix[i];
    }
    apply_viewport_projection(projection);
    renderer->SetMvpMatrix(&projection);
    renderer->SetModelColor(1.0f, 1.0f, 1.0f, _model->GetModelOpacity());
    renderer->DrawModel();
    renderer->PostDraw();
    visual_state_ = BongoCatLive2DVisualState{};
    visual_state_.fit_scale = frame_fit_scale_;
    visual_state_.fitted = frame_fit_scale_ < 0.9999f;
    visual_state_.mver_projection = render_options_.mver_projection;
    visual_projection_.SetMatrix(projection.GetArray());
    visual_state_cached_ = false;
    visual_state_ready_ = true;
}

#endif // BONGO_CAT_HAS_CUBISM_VULKAN

} // namespace bongo_cat
