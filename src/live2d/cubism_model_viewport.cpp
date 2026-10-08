#include "cubism_plugin_services.hpp"
#include "cubism_model.hpp"
#include "model_frame_policy.h"

#include <algorithm>
#include <cmath>

namespace bongo_cat {

void NativeModel::update_viewport() {
    if (tight_frame_) {
        /* The tight draw remaps the cropped region onto the whole window and
           the native window is sized to that crop: no letterbox. Reporting a
           sub-rect here both squeezes the GL glViewport to one side and feeds
           the pointer/corner code a false content rect. */
        viewport_x_ = 0;
        viewport_y_ = 0;
        viewport_width_ = std::max(1, width_);
        viewport_height_ = std::max(1, height_);
        /* The 2D overlay layers must follow the model's draw mapping, not the
           window: the canvas occupies a frame_-derived sub-rect (its cropped
           edges extend past the window). Otherwise the desk art stretches to
           the crop's aspect while the pet stays at content scale and the two
           layers slide apart — the reported tight-mode misalignment. */
        float span_x = 1.0f + frame_.left + frame_.right;
        float span_y = 1.0f + frame_.top + frame_.bottom;
        span_x = std::max(span_x, 0.01f);
        span_y = std::max(span_y, 0.01f);
        overlay_width_ = std::max(1, (int)std::lround(width_ / span_x));
        overlay_height_ = std::max(1, (int)std::lround(height_ / span_y));
        overlay_x_ = (int)std::lround(width_ * frame_.left / span_x);
        int overlay_top = (int)std::lround(height_ * frame_.top / span_y);
        overlay_y_ = std::max(1, height_) - overlay_top - overlay_height_;
        frame_fit_scale_ = 1.0f;
        return;
    }
    BongoCatFrameViewport v = bongo_cat_frame_viewport(frame_, required_frame_,
        std::max(1, width_), std::max(1, height_), vertical_flip_);
    viewport_x_ = v.x;
    viewport_y_ = v.y;
    viewport_width_ = v.width;
    viewport_height_ = v.height;
    overlay_x_ = v.x;
    overlay_y_ = v.y;
    overlay_width_ = v.width;
    overlay_height_ = v.height;
    frame_fit_scale_ = v.scale;
}

void NativeModel::set_frame(const BongoCatModelRuntimeFrame &frame) {
    if (!bongo_cat_frame_valid(frame)) return;
    frame_ = frame;
    update_viewport();
}

bool NativeModel::frame(BongoCatModelRuntimeFrame *frame) const {
    if (!_model || !frame) return false;
    *frame = frame_;
    return true;
}

bool NativeModel::viewport(int *x, int *y, int *width, int *height) const {
    if (!_model || !x || !y || !width || !height) return false;
    *x = viewport_x_;
    *y = viewport_y_;
    *width = viewport_width_;
    *height = viewport_height_;
    return true;
}

bool NativeModel::overlay_viewport(int *x, int *y, int *width,
    int *height) const {
    if (!_model || !x || !y || !width || !height) return false;
    *x = overlay_x_;
    *y = overlay_y_;
    *width = overlay_width_;
    *height = overlay_height_;
    return true;
}

void NativeModel::apply_viewport_projection(
    Csm::CubismMatrix44 &projection) const {
    if (width_ <= 0 || height_ <= 0 || viewport_width_ <= 0 ||
        viewport_height_ <= 0) return;
    float scale_x = (float)viewport_width_ / (float)width_;
    float scale_y = (float)viewport_height_ / (float)height_;
    float translate_x = (2.0f * viewport_x_ + viewport_width_) /
        (float)width_ - 1.0f;
    float translate_y = (2.0f * viewport_y_ + viewport_height_) /
        (float)height_ - 1.0f;
    float *matrix = projection.GetArray();
    matrix[0] *= scale_x;
    matrix[4] *= scale_x;
    matrix[12] = matrix[12] * scale_x + translate_x;
    matrix[1] *= scale_y;
    matrix[5] *= scale_y;
    matrix[13] = matrix[13] * scale_y + translate_y;
}

void NativeModel::record_visible_state(Csm::CubismMatrix44 &projection) const {
    visual_state_.drawable_count = _model->GetDrawableCount();
    for (int i = 0; i < _model->GetDrawableCount(); ++i) {
        if (_model->GetDrawableDynamicFlagIsVisible(i) &&
            _model->GetDrawableOpacity(i) > 0.001f)
            ++visual_state_.drawable_visible;
        if (_model->GetDrawableDynamicFlagVertexPositionsDidChange(i))
            ++visual_state_.drawable_vertex_changed;
    }
    visual_state_.offscreen_count = _model->GetOffscreenCount();
    for (int i = 0; i < _model->GetOffscreenCount(); ++i)
        if (_model->GetOffscreenOpacity(i) > 0.001f)
            ++visual_state_.offscreen_positive;
    visual_state_.part_count = _model->GetPartCount();
    for (int i = 0; i < _model->GetPartCount(); ++i)
        if (_model->GetPartOpacity(i) > 0.001f)
            ++visual_state_.part_positive;
    if (_model->GetModelOpacity() <= 0.001f) return;
    ModelBounds bounds = capture_visible_bounds();
    if (!bounds.valid) return;
    float x0 = projection.TransformX(bounds.min_x);
    float x1 = projection.TransformX(bounds.max_x);
    float y0 = projection.TransformY(bounds.min_y);
    float y1 = projection.TransformY(bounds.max_y);
    visual_state_.visible_min_x = std::min(x0, x1);
    visual_state_.visible_max_x = std::max(x0, x1);
    visual_state_.visible_min_y = std::min(y0, y1);
    visual_state_.visible_max_y = std::max(y0, y1);
    visual_state_.visible = true;
}

bool NativeModel::visual_state(BongoCatModelRuntimeVisualState *state) const {
    if (!state || !visual_state_ready_) return false;
    if (!visual_state_cached_) {
        // Bounds are used by pointer anchoring and visual audits, not drawing.
        // Avoid traversing every triangle on every animated frame.
        bool mver_projection = visual_state_.mver_projection;
        float fit_scale = visual_state_.fit_scale;
        visual_state_ = BongoCatModelRuntimeVisualState{};
        visual_state_.fit_scale = fit_scale;
        visual_state_.fitted = fit_scale < 0.9999f;
        visual_state_.mver_projection = mver_projection;
        record_visible_state(visual_projection_);
        visual_state_cached_ = true;
    }
    *state = visual_state_;
    return true;
}

} // namespace bongo_cat
