#include "cubism_plugin_services.hpp"
#include "cubism_model.hpp"
#include "cubism_native_texture.hpp"
#include "bongo_cat/safe_ffi.h"
#include "cubism_texture_resolution.hpp"
#include <SDL3/SDL_log.h>
#include <cstring>
#include <algorithm>
#include <memory>
#include <exception>
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
#include "cubism_vulkan_submission.hpp"
#endif

namespace bongo_cat {
namespace {
struct NativeTextureLoad {
    NativeTexturePixels *atlas = nullptr;
    const char *file = nullptr;
    TextureResolution bound{};
    bool bc7 = false, success = false;
    BongoCatError *error = nullptr;
    SDL_Thread *thread = nullptr;
    ~NativeTextureLoad() { if (thread) SDL_WaitThread(thread, nullptr); }
    void run(BongoCatModelRuntimeLoadProgress progress, void *userdata, float fraction) {
        const auto worker = [](void *userdata) -> int {
            auto &job = *static_cast<NativeTextureLoad *>(userdata);
            job.success = job.atlas->load(job.file, job.bound.max_width,
                job.bound.max_height, job.bc7, job.error);
            return job.success ? 0 : -1;
        };
        if (progress) thread = SDL_CreateThread(worker, BONGO_CAT_SLUG "-native-load", this);
        if (!thread) { worker(this); return; }
        while (SDL_GetThreadState(thread) == SDL_THREAD_ALIVE) {
            progress(userdata, fraction);
            SDL_Delay(2);
        }
        SDL_WaitThread(thread, nullptr);
        thread = nullptr;
    }
};
}
void NativeModel::set_rhi_info(const BongoCatRhiDeviceInfo &info) {
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
    bool rebuild = GetRenderer<Csm::Rendering::CubismRenderer>() &&
        info.backend == BONGO_CAT_RHI_VULKAN &&
        (info.image_count != rhi_info_.image_count ||
         info.extent_width != rhi_info_.extent_width ||
         info.extent_height != rhi_info_.extent_height ||
         info.color_format != rhi_info_.color_format);
    if (rebuild) {
        bongo_cat_rhi_wait_idle(static_cast<const BongoCatRhi *>(rhi_info_.rhi_handle));
        // Delete with the OLD SDK buffer count before installing new settings.
        release_renderer();
    }
#endif
    rhi_info_ = info;
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
    if (rebuild) {
        BongoCatError error{};
        try {
            if (!create_renderer_vulkan(&error))
                SDL_LogError(SDL_LOG_CATEGORY_RENDER, "%s", error.message);
        } catch (const std::exception &exception) {
            release_renderer();
            SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Cannot rebuild Vulkan renderer: %s", exception.what());
        }
    }
#endif
}
const BongoCatRhiDeviceInfo &NativeModel::rhi_info() const { return rhi_info_; }

bool NativeModel::load_textures_native(BongoCatError *error,
    BongoCatModelRuntimeLoadProgress progress, void *userdata, int display_width, int display_height) {
    release_render_resources();
    int count = setting_->GetTextureCount();
    native_alpha_.resize((size_t)count);
    native_texture_info_.resize((size_t)count);
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
    if (rhi_info_.backend == BONGO_CAT_RHI_VULKAN)
        vulkan_textures_.resize((size_t)count);
#endif
    int reference_width = 0, reference_height = 0;
    canvas_size(&reference_width, &reference_height);
    if (render_options_.mver_projection) {
        reference_width = render_options_.reference_width;
        reference_height = render_options_.reference_height;
    }
    if (reference_width <= 0) reference_width = width_;
    if (reference_height <= 0) reference_height = height_;
    if (display_width <= 0 || display_height <= 0) {
        display_width = viewport_width_; display_height = viewport_height_;
    }
    int limit = 16384;
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
    if (rhi_info_.backend == BONGO_CAT_RHI_VULKAN) {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties((VkPhysicalDevice)rhi_info_.vulkan_physical_device, &properties);
        limit = std::min(limit, (int)properties.limits.maxImageDimension2D);
    }
#endif
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
    struct UploadBatch {
        bool active;
        ~UploadBatch() { if (active) vulkan_submit_cancel(); }
    } upload_batch{rhi_info_.backend == BONGO_CAT_RHI_VULKAN};
    if (upload_batch.active) {
        vulkan_submit_initialize((VkDevice)rhi_info_.vulkan_device, (VkQueue)rhi_info_.vulkan_queue,
            (VkCommandPool)rhi_info_.vulkan_command_pool);
        vulkan_upload_batch_begin();
    }
#endif
    texture_limit_ = limit;
    for (int i = 0; i < count; ++i) {
        int width = 0, height = 0;
        const auto file = path(setting_->GetTextureFileName(i));
        if (!bongo_cat_image_info(file.c_str(), &width, &height) || width <= 0 || height <= 0) {
            bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Cannot read native texture dimensions");
            release_render_resources(); return false;
        }
        auto bound = texture_resolution_for(dynamic_texture_resolution_, display_width, display_height,
            reference_width, reference_height, width, height, limit, render_quality_percent_);
        auto atlas = std::make_unique<NativeTexturePixels>();
        const bool bc7 = rhi_info_.backend == BONGO_CAT_RHI_VULKAN &&
            (native_vulkan_features_ & BONGO_CAT_VULKAN_TEXTURE_COMPRESSION_BC) != 0;
        NativeTextureLoad loading{atlas.get(), file.c_str(), bound, bc7, false, error};
        loading.run(progress, userdata, .50f + .45f * (float)i / (float)(count > 0 ? count : 1));
        if (!loading.success) {
            release_render_resources(); return false;
        }
        auto &metadata = native_texture_info_[(size_t)i];
        width = (int)atlas->data.width; height = (int)atlas->data.height;
        bool uploaded = false;
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
        if (rhi_info_.backend == BONGO_CAT_RHI_VULKAN)
            uploaded = upload_texture_vulkan(i, atlas->data.pixels, width, height, error,
                atlas->data.format, atlas->data.size);
        if (!uploaded && rhi_info_.backend == BONGO_CAT_RHI_VULKAN && atlas->data.format == 1) {
            if (atlas->load(file.c_str(), bound.max_width, bound.max_height, false, error))
                uploaded = upload_texture_vulkan(i, atlas->data.pixels,
                    (int)atlas->data.width, (int)atlas->data.height, error,
                    atlas->data.format, atlas->data.size);
        }
#endif
#ifdef BONGO_CAT_HAS_CUBISM_METAL
        if (rhi_info_.backend == BONGO_CAT_RHI_METAL) {
            uploaded = upload_texture_metal(i, atlas->data.pixels, width, height, error);
        }
#endif
        if (!uploaded) { release_render_resources(); return false; }
        if (error) *error = {};
        width = (int)atlas->data.width; height = (int)atlas->data.height;
        metadata.source_width = (int)atlas->data.source_width;
        metadata.source_height = (int)atlas->data.source_height;
        atlas->alpha(native_alpha_[(size_t)i]);
        metadata.bytes = atlas->data.size;
        native_texture_bytes_ += metadata.bytes;
        atlas.reset();
        metadata.width = width; metadata.height = height;
        if (progress) progress(userdata, .50f + .45f * (float)(i + 1) /
            (float)(count > 0 ? count : 1));
    }
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
    if (upload_batch.active) {
        vulkan_upload_batch_end();
        // Hidden models may never draw: retire the final staging slots now.
        vulkan_submit_wait();
        upload_batch.active = false;
    }
#endif
    if (!frame_prepared_) { prepare_expression_frame(); frame_prepared_ = true; }
    prepare_frame_bounds();
    if (!create_renderer(error)) { release_render_resources(); return false; }
    // One mip level; native atlases honor the display/quality bounds at load time.
    return true;
}

void NativeModel::prepare_native_mask_sizes() {
    // Bound atlas memory while retaining the same layout and resize hysteresis.
    const int limit = std::min(texture_limit_ > 0 ? texture_limit_ : 16384, 2048);
    drawable_masks_.size = mask_target_size(drawable_masks_.layout,
        viewport_width_, viewport_height_, limit, drawable_masks_.size);
    offscreen_masks_.size = mask_target_size(offscreen_masks_.layout,
        viewport_width_, viewport_height_, limit, offscreen_masks_.size);
}

void NativeModel::native_projection(Csm::CubismMatrix44 &projection) {
    bool tight = tight_frame_ && tight_reference_content_width_ > 0 &&
        tight_reference_content_height_ > 0;
    build_projection(projection, tight ? tight_reference_content_width_ : viewport_width_,
        tight ? tight_reference_content_height_ : viewport_height_);
    float *m = projection.GetArray();
    if (vertical_flip_) for (int i = 1; i < 16; i += 4) m[i] = -m[i];
    if (tight) {
        float sx = 1.0f + frame_.left + frame_.right;
        float sy = 1.0f + frame_.top + frame_.bottom;
        m[0] /= sx; m[4] /= sx; m[12] = (m[12] - frame_.right + frame_.left) / sx;
        m[1] /= sy; m[5] /= sy; m[13] = (m[13] - frame_.top + frame_.bottom) / sy;
    } else apply_viewport_projection(projection);
    if (tight) update_tight_frame(projection);
}
void NativeModel::native_visual_state(Csm::CubismMatrix44 &projection) {
    visual_state_ = BongoCatModelRuntimeVisualState{};
    visual_state_.fit_scale = frame_fit_scale_;
    visual_state_.fitted = frame_fit_scale_ < 0.9999f;
    visual_state_.mver_projection = render_options_.mver_projection;
    visual_projection_.SetMatrix(projection.GetArray());
    visual_state_cached_ = false;
    visual_state_ready_ = true;
}
} // namespace bongo_cat
