#include "cubism_plugin_services.hpp"
#include "cubism_model.hpp"

#include <algorithm>
#include <cmath>

#include <SDL3/SDL_log.h>

namespace bongo_cat {

void NativeModel::resize(int width, int height) {
    if (width <= 0 || height <= 0) return;
    bool changed = width != width_ || height != height_;
    width_ = width;
    height_ = height;
    update_viewport();
    if (changed) schedule_texture_refresh();
    if (!_model || (width == renderer_width_ && height == renderer_height_)) return;
    if (!_model->IsBlendModeEnabled()) {
        renderer_width_ = width_;
        renderer_height_ = height_;
        return;
    }
    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO,
        "Live2D render target resized in place: %dx%d -> %dx%d",
        renderer_width_, renderer_height_, width_, height_);
    SetRenderTargetSize((Csm::csmUint32)width_, (Csm::csmUint32)height_);
    renderer_width_ = width_;
    renderer_height_ = height_;
}

void NativeModel::reshape(int width, int height) {
    if (width > 0 && height > 0) {
        bool changed = width != width_ || height != height_;
        width_ = width;
        height_ = height;
        update_viewport();
        if (changed) schedule_texture_refresh();
    }
}

void NativeModel::build_projection(Csm::CubismMatrix44 &projection,
    int width, int height) {
    if (!_model || width <= 0 || height <= 0) return;
    Csm::CubismModelMatrix model_matrix(*_modelMatrix);
    if (render_options_.mver_projection) {
        float aspect = (float)render_options_.reference_width /
            (float)render_options_.reference_height;
        projection.Scale((mirror_ ? -1.0f : 1.0f) *
            render_options_.projection_scale,
            render_options_.projection_scale * aspect);
        projection.Translate(render_options_.offset_x, render_options_.offset_y);
    } else if (_model->GetCanvasWidth() > 1.0f && width < height) {
        model_matrix.SetWidth(2.0f);
        projection.Scale(mirror_ ? -1.0f : 1.0f,
            (float)width / (float)height);
    } else {
        projection.Scale((mirror_ ? -1.0f : 1.0f) *
            (float)height / (float)width, 1.0f);
    }
    projection.MultiplyByMatrix(&model_matrix);
    if (render_options_.mver_projection) {
        // Mver 0.1.6's Core reports canvas units at twice the modern scale.
        // Preserve authored Layout translation while matching its model scale.
        float *matrix = projection.GetArray();
        constexpr float mver_core_canvas_scale = 0.5f;
        matrix[0] *= mver_core_canvas_scale;
        matrix[1] *= mver_core_canvas_scale;
        matrix[4] *= mver_core_canvas_scale;
        matrix[5] *= mver_core_canvas_scale;
    }
}

void NativeModel::set_mirror(bool mirror) { mirror_ = mirror; }
void NativeModel::set_vertical_flip(bool flipped) {
    if (vertical_flip_ == flipped) return;
    vertical_flip_ = flipped;
    update_viewport();
}
void NativeModel::set_tight_frame(bool tight) {
    SDL_LogInfo(SDL_LOG_CATEGORY_CUSTOM, "[tight] set_tight_frame=%d (was %d)",
        tight ? 1 : 0, tight_frame_ ? 1 : 0);
    if (tight_frame_ == tight) return;
    tight_frame_ = tight;
    tight_observed_ = false;
    tight_observe_frames_ = 0;
    tight_locked_ = false;
    tight_geom_settle_ = 0;
    tight_geom_last_width_ = tight_geom_last_height_ = -1;
    tight_anim_last_ns_ = 0;
    tight_shrink_due_ns_ = 0;
    if (!tight) {
        /* Restore the frame captured when tight mode was enabled; observe
           accumulation continues from there instead of from the crop. */
        required_frame_ = tight_reference_frame_;
        return;
    }
    /* Freeze the measurement reference on every enable (and on model
       switches, which re-apply the setting): the envelope projection must
       not follow the cropped window, or its aspect-dependent fit branch
       flips and feeds the crop back into itself. Derive the frozen content
       size from THIS model's canvas, never from the window: the window may
       still be the previous model's, and dividing it by the inherited frame
       froze the old aspect — the pet rendered skewed against the overlay
       art after every model switch. */
    tight_reference_frame_ = frame_;
    /* Hold the window exactly where it is during the observe phase so the
       bounds union is sampled from a still frame. */
    required_frame_ = frame_;
    if (!canvas_size(&tight_reference_content_width_,
        &tight_reference_content_height_)) {
        tight_reference_content_width_ = (int)std::max(1.0,
            std::round(width_ / (1.0 + frame_.left + frame_.right)));
        tight_reference_content_height_ = (int)std::max(1.0,
            std::round(height_ / (1.0 + frame_.top + frame_.bottom)));
    }
}

void NativeModel::set_tight_overlay_rect(const float *rect) {
    bool valid = rect && std::isfinite(rect[0]) && std::isfinite(rect[1]) &&
        std::isfinite(rect[2]) && std::isfinite(rect[3]) &&
        rect[0] < rect[2] && rect[1] < rect[3];
    if (!valid) {
        if (tight_overlay_valid_)
            SDL_LogInfo(SDL_LOG_CATEGORY_CUSTOM,
                "[tight] overlay art rect cleared");
        tight_overlay_valid_ = false;
        return;
    }
    tight_overlay_valid_ = true;
    tight_overlay_rect_[0] = std::max(-1.0f, rect[0]);
    tight_overlay_rect_[1] = std::max(-1.0f, rect[1]);
    tight_overlay_rect_[2] = std::min(1.0f, rect[2]);
    tight_overlay_rect_[3] = std::min(1.0f, rect[3]);
    SDL_LogInfo(SDL_LOG_CATEGORY_CUSTOM,
        "[tight] overlay art rect x=%.3f..%.3f y=%.3f..%.3f",
        tight_overlay_rect_[0], tight_overlay_rect_[2],
        tight_overlay_rect_[1], tight_overlay_rect_[3]);
}

void NativeModel::set_render_options(const BongoCatModelRuntimeRenderOptions &options) {
    render_options_ = options;
}

} // namespace bongo_cat
