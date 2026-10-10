#include "cubism_plugin_services.hpp"
#include "cubism_model.hpp"
#include "bongo_cat/safe_ffi.h"
#include "cubism_texture_resolution.hpp"
#include <SDL3/SDL_log.h>
#include <cstring>
#include <memory>
#include <exception>

namespace bongo_cat {
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
    if (display_width <= 0 || display_height <= 0) {
        display_width = viewport_width_; display_height = viewport_height_;
    }
    int limit = 16384;
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
    if (rhi_info_.backend == BONGO_CAT_RHI_VULKAN) {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties((VkPhysicalDevice)rhi_info_.vulkan_physical_device, &properties);
        limit = (int)properties.limits.maxImageDimension2D;
    }
#endif
    for (int i = 0; i < count; ++i) {
        int width = 0, height = 0;
        auto *decoded = [this, i, &width, &height] {
            auto bytes = read(path(setting_->GetTextureFileName(i)));
            return bongo_safe_image_decode(bytes.data(), bytes.size(), &width, &height);
        }();
        size_t texels = width > 0 && height > 0 ? (size_t)width * (size_t)height : 0;
        const auto free_pixels = [texels](unsigned char *pixels) {
            if (pixels) bongo_safe_free_pixels(pixels, texels * 4);
        };
        std::unique_ptr<unsigned char, decltype(free_pixels)> owned(decoded, free_pixels);
        if (!decoded || !texels || texels > SIZE_MAX / 4) {
            bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
                "Cannot decode Live2D texture %d", i);
            release_render_resources();
            return false;
        }
        BongoCatImage image{};
        image.pixels = decoded; image.width = width; image.height = height;
        bongo_cat_image_make_alpha_mask(&image, &native_alpha_[(size_t)i]);
        auto bound = texture_resolution_for(dynamic_texture_resolution_, display_width, display_height,
            reference_width, reference_height, width, height, limit, render_quality_percent_);
        BongoCatImage resized{};
        const auto free_image = [](BongoCatImage *value) { bongo_cat_image_free(value); };
        std::unique_ptr<BongoCatImage, decltype(free_image)> resized_owner(&resized, free_image);
        image.pixels_ffi = true;
        image.pixels = owned.release();
        if (!bongo_cat_image_resize_rgba_take(&image, bound.max_width, bound.max_height, &resized, error)) {
            bongo_cat_image_free(&image);
            release_render_resources(); return false;
        }
        width = resized.width; height = resized.height;
        const size_t pixel_bytes = (size_t)width * (size_t)height * 4;
        // Keep the resize result owned until the synchronous upload completes.
        auto *pixels = resized.pixels;
        for (size_t p = 0; p < pixel_bytes; p += 4) {
            unsigned alpha = pixels[p + 3];
            for (size_t c = 0; c < 3; ++c)
                pixels[p + c] = (unsigned char)(pixels[p + c] * alpha / 255);
        }
        bool uploaded = false;
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
        if (rhi_info_.backend == BONGO_CAT_RHI_VULKAN)
            uploaded = upload_texture_vulkan(i, pixels, width, height, error);
#endif
#ifdef BONGO_CAT_HAS_CUBISM_METAL
        if (rhi_info_.backend == BONGO_CAT_RHI_METAL) {
            std::vector<unsigned char> metal_pixels(pixels, pixels + pixel_bytes);
            resized_owner.reset();
            uploaded = upload_texture_metal(i, metal_pixels, width, height, error);
        }
#endif
        if (!uploaded) { release_render_resources(); return false; }
        resized_owner.reset();
        native_texture_bytes_ += pixel_bytes;
        if (progress) progress(userdata, .50f + .45f * (float)(i + 1) /
            (float)(count > 0 ? count : 1));
    }
    if (!frame_prepared_) { prepare_expression_frame(); frame_prepared_ = true; }
    prepare_frame_bounds();
    if (!create_renderer(error)) { release_render_resources(); return false; }
    // One mip level; native atlases honor the display/quality bounds at load time.
    return true;
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
