#include "cubism_plugin_services.hpp"
#include "cubism_native_masks.hpp"
#include "cubism_runtime.hpp"
#include <Rendering/Vulkan/CubismClass_Vulkan.hpp>
#include "cubism_vulkan_memory.hpp"
#include "cubism_vulkan_recording.hpp"
#include "cubism_vulkan_bindless.hpp"
#include "cubism_vulkan_submission.hpp"
#include <exception>
#include <cstring>
#include <stdexcept>
#include "bongo_cat/log.h"

namespace bongo_cat {
static void log_vulkan_memory(const char *stage) {
    const auto usage = vulkan_memory_usage();
    SDL_LogInfo(BONGO_CAT_LOG_LIFECYCLE,
        "[vulkan-memory] stage=%s used_bytes=%llu reserved_bytes=%llu heap_usage_bytes=%llu "
        "heap_budget_bytes=%llu buffers=%zu images=%zu",
        stage, (unsigned long long)usage.used, (unsigned long long)usage.reserved,
        (unsigned long long)usage.heap_usage, (unsigned long long)usage.heap_budget,
        usage.buffers, usage.images);
    SDL_LogInfo(BONGO_CAT_LOG_LIFECYCLE,
        "[vulkan-memory] device_usage_bytes=%llu device_budget_bytes=%llu atlas_bytes=%llu "
        "target_bytes=%llu geometry_bytes=%llu uniform_bytes=%llu staging_bytes=%llu",
        (unsigned long long)usage.device_usage, (unsigned long long)usage.device_budget,
        (unsigned long long)usage.atlas_bytes, (unsigned long long)usage.target_bytes,
        (unsigned long long)usage.geometry_bytes, (unsigned long long)usage.uniform_bytes,
        (unsigned long long)usage.staging_bytes);
}
uint32_t configure_vulkan_optimization(BongoCatModelRuntime *runtime,
    uint32_t requested, uint32_t backend) {
    if (!runtime || runtime->model) return 0;
    const auto &info = runtime->rhi_info;
    const bool vulkan = backend == BONGO_CAT_RHI_VULKAN && info.backend == BONGO_CAT_RHI_VULKAN;
    const bool allocation = vulkan && (requested & BONGO_CAT_OPTIMIZE_LARGE_ALLOCATION) != 0;
    const bool system_memory = vulkan && (requested & BONGO_CAT_OPTIMIZE_SYSTEM_MEMORY) != 0;
    uint32_t active = 0;
    if (bongo_cat::vulkan_memory_configure((VkInstance)info.vulkan_instance,
            (VkPhysicalDevice)info.vulkan_physical_device,
            (VkDevice)info.vulkan_device, vulkan,
            allocation, system_memory,
            (runtime->vulkan_features & BONGO_CAT_VULKAN_MEMORY_BUDGET) != 0)) {
        if (allocation) active |= BONGO_CAT_OPTIMIZE_LARGE_ALLOCATION;
        if (system_memory) active |= BONGO_CAT_OPTIMIZE_SYSTEM_MEMORY;
    }
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
    vulkan_submit_wait();
    class RendererAccess : public Csm::Rendering::CubismRenderer_Vulkan {
    public:
        using CubismRenderer_Vulkan::DoStaticRelease;
    };
    vulkan_recording_release();
    vulkan_bindless_release();
    RendererAccess::DoStaticRelease();
    vulkan_submit_release();
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
    vulkan_submit_initialize((VkDevice)info.vulkan_device, (VkQueue)info.vulkan_queue,
        (VkCommandPool)info.vulkan_command_pool, !vulkan_memory_prefers_device());
    Csm::Rendering::CubismRenderer_Vulkan::SetConstantSettings(
        (VkDevice)info.vulkan_device, (VkPhysicalDevice)info.vulkan_physical_device,
        (VkCommandPool)info.vulkan_command_pool, (VkQueue)info.vulkan_queue,
        info.image_count, VkExtent2D{info.extent_width, info.extent_height},
        VK_NULL_HANDLE, (VkFormat)info.color_format, (VkFormat)info.depth_format);
    std::vector<VkDescriptorImageInfo> atlases;
    atlases.reserve(vulkan_textures_.size());
    for (const auto &texture : vulkan_textures_)
        atlases.push_back({texture.GetSampler(), texture.GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    vulkan_bindless_model(atlases.data(), atlases.size(),
        _model && !_model->IsBlendModeEnabled() && _model->GetOffscreenCount() == 0);
    release_renderer();
    std::vector<int> vertex_counts;
    vertex_counts.reserve((size_t)_model->GetDrawableCount());
    for (int i = 0; i < _model->GetDrawableCount(); ++i)
        vertex_counts.push_back(_model->GetDrawableVertexCount(i));
    vulkan_geometry_ = vulkan_geometry_create((VkDevice)info.vulkan_device,
        (VkPhysicalDevice)info.vulkan_physical_device, vertex_counts.data(), vertex_counts.size());
    VulkanGeometryScope geometry_scope(vulkan_geometry_);
    vulkan_upload_batch_begin();
    try {
        const int masks = prepare_mask_layout();
        NativeMaskCountScope mask_counts(drawable_masks_.count, offscreen_masks_.count);
        CreateRenderer((Csm::csmUint32)width_, (Csm::csmUint32)height_, masks);
        vulkan_upload_batch_end();
        // Renderer initialization also uploads indices. Retire their staging
        // before returning, including when the model will initially be hidden.
        vulkan_submit_wait();
    } catch (...) { vulkan_submit_cancel(); throw; }
    auto *renderer = GetRenderer<Csm::Rendering::CubismRenderer_Vulkan>();
    if (!renderer) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_CUBISM, "Cannot create Live2D Vulkan renderer");
        return false;
    }
    for (auto &texture : vulkan_textures_) renderer->BindTexture(texture);
    renderer->IsPremultipliedAlpha(true);
    renderer_width_ = width_; renderer_height_ = height_;
    log_vulkan_memory("renderer-ready");
    return true;
}

bool NativeModel::upload_texture_vulkan(int index,
    const unsigned char *pixels, int width, int height, BongoCatError *error,
    uint32_t texture_format, size_t bytes) {
    if (!pixels || width <= 0 || height <= 0 || index < 0 || (size_t)index >= vulkan_textures_.size()) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_ARGUMENT, "Invalid Vulkan texture upload");
        return false;
    }
    VkDevice device = (VkDevice)rhi_info_.vulkan_device;
    VkPhysicalDevice physical = (VkPhysicalDevice)rhi_info_.vulkan_physical_device;
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physical, &properties);
    if ((uint32_t)width > properties.limits.maxImageDimension2D ||
        (uint32_t)height > properties.limits.maxImageDimension2D) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM, "Live2D texture exceeds Vulkan size limit");
        return false;
    }
    if (vulkan_textures_[(size_t)index].GetImage()) vulkan_submit_wait();
    else vulkan_draw_wait(); // No pending draw/upload may reference a replaced atlas.
    struct Staging {
        Csm::CubismBufferVulkan buffer;
        VkDevice device;
        VkBuffer pooled = VK_NULL_HANDLE;
        VkDeviceMemory pooled_memory = VK_NULL_HANDLE;
        VkBuffer handle() const { return pooled ? pooled : buffer.GetBuffer(); }
        ~Staging() {
            if (pooled && !vulkan_upload_defer_buffer(pooled, pooled_memory)) vulkan_buffer_destroy(pooled);
            else if (!pooled) buffer.Destroy(device);
        }
    } staging{{}, device};
    struct Replacement {
        Csm::CubismImageVulkan image;
        VkDevice device;
        ~Replacement() { image.Destroy(device); }
    } replacement{{}, device};
    try {
        const VkFormat format = texture_format == 1 ? VK_FORMAT_BC7_UNORM_BLOCK : VK_FORMAT_R8G8B8A8_UNORM;
        const VkDeviceSize size = texture_format == 1 ?
            ((VkDeviceSize)width + 3) / 4 * (((VkDeviceSize)height + 3) / 4) * 16 :
            (VkDeviceSize)width * (VkDeviceSize)height * 4;
        if (texture_format > 1 || (bytes && size != bytes))
            throw std::runtime_error("Invalid Vulkan texture payload size");
        VkFormatProperties format_properties{};
        vkGetPhysicalDeviceFormatProperties(physical, format, &format_properties);
        const VkFormatFeatureFlags needed = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
            VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
        if ((format_properties.optimalTilingFeatures & needed) != needed ||
            (texture_format == 1 && !(native_vulkan_features_ & BONGO_CAT_VULKAN_TEXTURE_COMPRESSION_BC)))
            throw std::runtime_error("Vulkan texture format is unavailable");
        const VkMemoryPropertyFlags memory_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        // Dedicated staging is retained by its upload slot until Fence completion.
        vulkan_upload_budget(size);
        if (vulkan_buffer_create(device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                memory_flags, &staging.pooled, &staging.pooled_memory, true)) {
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
        auto &image = replacement.image;
        image.CreateImage(device, physical, width, height, 1, format,
            VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        image.CreateView(device, format, VK_IMAGE_ASPECT_COLOR_BIT, 1);
        // No anisotropy feature is required for the one-level linear sampler.
        image.CreateSampler(device, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST, 0.0f, 1);
        auto *rhi = static_cast<const BongoCatRhi *>(rhi_info_.rhi_handle);
        VkCommandBuffer command = vulkan_upload_begin();
        if (!command) throw std::runtime_error("Cannot allocate Vulkan upload commands");
        image.SetImageLayout(command, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, VK_IMAGE_ASPECT_COLOR_BIT);
        VkBufferImageCopy region{};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = VkExtent3D{(uint32_t)width, (uint32_t)height, 1};
        vkCmdCopyBufferToImage(command, staging.handle(), image.GetImage(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        image.SetImageLayout(command, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, VK_IMAGE_ASPECT_COLOR_BIT);
        vulkan_submit(command, VK_NULL_HANDLE, VK_NULL_HANDLE);
        auto *renderer = GetRenderer<Csm::Rendering::CubismRenderer_Vulkan>();
        if (renderer) {
            (void)rhi; // Previous draw fence was waited before replacement; upload is queued.
            std::vector<VkDescriptorImageInfo> atlases;
            atlases.reserve(vulkan_textures_.size());
            for (size_t i = 0; i < vulkan_textures_.size(); ++i) {
                const auto &atlas = i == (size_t)index ? image : vulkan_textures_[i];
                atlases.push_back({atlas.GetSampler(), atlas.GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
            }
            vulkan_bindless_model(atlases.data(), atlases.size(),
                !_model->IsBlendModeEnabled() && _model->GetOffscreenCount() == 0);
            struct Rebind {
                explicit Rebind(int slot) { vulkan_texture_rebind_slot = slot; }
                ~Rebind() { vulkan_texture_rebind_slot = -1; }
            } rebind(index);
            renderer->BindTexture(image);
        }
        std::swap(vulkan_textures_[(size_t)index], image);
        SDL_LogInfo(BONGO_CAT_LOG_LIFECYCLE, "[vulkan-atlas] index=%d size=%dx%d format=%s bytes=%llu",
            index, width, height, texture_format == 1 ? "bc7" : "rgba8", (unsigned long long)size);
        return true;
    } catch (const std::exception &exception) {
        vulkan_submit_cancel();
        bongo_cat_error_set(error, BONGO_CAT_ERROR_CUBISM, "%s", exception.what());
        return false;
    }
}

void NativeModel::release_textures_vulkan() {
    if (vulkan_textures_.empty()) return;
    vulkan_submit_wait();
    VkDevice device = (VkDevice)rhi_info_.vulkan_device;
    for (auto &texture : vulkan_textures_) texture.Destroy(device);
    vulkan_textures_.clear();
    log_vulkan_memory("textures-released");
}

void NativeModel::draw_vulkan() {
    auto *renderer = GetRenderer<Csm::Rendering::CubismRenderer_Vulkan>();
    if (!_model || !renderer || width_ <= 0 || height_ <= 0) return;
    vulkan_draw_wait(); // Retire before mutating shared geometry, masks or descriptors.
    vulkan_memory_next_frame();
    BongoCatRhiVulkanFrameInfo frame{};
    if (!bongo_cat_rhi_get_vulkan_frame_info(
        static_cast<const BongoCatRhi *>(rhi_info_.rhi_handle), &frame)) return;
    Csm::Rendering::CubismRenderer_Vulkan::SetRenderTarget(
        (VkImage)frame.image, (VkImageView)frame.view,
        (VkFormat)rhi_info_.color_format, VkExtent2D{rhi_info_.extent_width, rhi_info_.extent_height});
    prepare_native_mask_sizes();
    NativeMaskScope mask_scope(drawable_masks_.size, offscreen_masks_.size);
    Csm::CubismMatrix44 projection;
    native_projection(projection);
    renderer->SetMvpMatrix(&projection);
    renderer->SetModelColor(1.0f, 1.0f, 1.0f, _model->GetModelOpacity());
    VulkanGeometryScope geometry_scope(vulkan_geometry_);
    try {
        vulkan_geometry_prepare_begin();
        for (int i = 0; i < _model->GetDrawableCount(); ++i)
            vulkan_geometry_prepare((size_t)i, _model->GetDrawableVertices(i),
                reinterpret_cast<const float *>(_model->GetDrawableVertexUvs(i)));
        renderer->DrawModel();
        vulkan_geometry_commit(true);
    }
    catch (...) { vulkan_geometry_commit(false); vulkan_recording_cancel(); vulkan_submit_cancel(); throw; }
    native_visual_state(projection);
}
} // namespace bongo_cat
