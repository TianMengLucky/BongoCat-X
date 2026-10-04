#include "cubism_model.hpp"

#include <algorithm>
#include <cmath>
#include "bongo_cat/model_memory.h"
#include "bongo_cat/resource_trace.h"
#include "cubism_target_bindings.hpp"
#include "bongo_cat/gl_api.h"
#include "model_frame_policy.h"

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>
#include <Rendering/OpenGL/CubismOffscreenManager_OpenGLES2.hpp>

namespace bongo_cat {

namespace {
class DrawFrame final {
public:
    explicit DrawFrame(Csm::Rendering::CubismOffscreenManager_OpenGLES2 *manager)
        : manager_(manager), srgb_(glIsEnabled(GL_FRAMEBUFFER_SRGB)) {
        manager_->BeginFrameProcess();
        // Cubism's RGBA8 blend equations operate on authored color values.
        if (srgb_) glDisable(GL_FRAMEBUFFER_SRGB);
        // Modern blend targets may also be allocated lazily inside DrawModel.
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpack_);
        if (unpack_) glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }
    ~DrawFrame() {
        if (unpack_) glBindBuffer(GL_PIXEL_UNPACK_BUFFER, (GLuint)unpack_);
        if (srgb_) glEnable(GL_FRAMEBUFFER_SRGB);
        manager_->EndFrameProcess();
    }
    DrawFrame(const DrawFrame &) = delete;
    DrawFrame &operator=(const DrawFrame &) = delete;
private:
    Csm::Rendering::CubismOffscreenManager_OpenGLES2 *manager_;
    GLboolean srgb_;
    GLint unpack_ = 0;
};
} // namespace

void NativeModel::draw() {
    auto *renderer = GetRenderer<Csm::Rendering::CubismRenderer_OpenGLES2>();
    if (!_model || !renderer || width_ <= 0 || height_ <= 0) return;
#ifdef CSM_TARGET_MAC_GL
    CoreProfileBinding binding(core_buffers_);
#endif
    const bool first_frame = trim_offscreen_pool_;
    const bool resources_changed = first_frame ||
        width_ != mask_last_width_ || height_ != mask_last_height_;
    update_mask_buffers();
    auto *manager = Csm::Rendering::CubismOffscreenManager_OpenGLES2::GetInstance();
    Csm::CubismMatrix44 projection;
    /* Tight mode builds the SAME projection the envelope was measured with
       (frozen content dims), then maps the cropped region onto the whole
       window. Measure and draw can then never disagree about where the pet
       is, regardless of model type or window aspect. */
    bool tight_projection = tight_frame_ &&
        tight_reference_content_width_ > 0 &&
        tight_reference_content_height_ > 0;
    build_projection(projection,
        tight_projection ? tight_reference_content_width_ : viewport_width_,
        tight_projection ? tight_reference_content_height_ : viewport_height_);
    if (vertical_flip_) {
        // Reflect the complete projection, including authored translation.
        float *matrix = projection.GetArray();
        for (int i = 1; i < 16; i += 4) matrix[i] = -matrix[i];
    }
    if (tight_projection) {
        /* NDC window region: x in [-1-2*left, 1+2*right], y in
           [-1-2*bottom, 1+2*top]. Rescale it onto [-1, 1].
           Use the ALLOCATED frame_, not required_frame_: while the geometry
           is animating toward its target they differ, and remapping by the
           target would stretch the pet inside the not-yet-resized window. */
        float *m = projection.GetArray();
        float span_x = 1.0f + frame_.left + frame_.right;
        float span_y = 1.0f + frame_.top + frame_.bottom;
        float center_x = frame_.right - frame_.left;
        float center_y = frame_.top - frame_.bottom;
        m[0] /= span_x; m[4] /= span_x;
        m[12] = (m[12] - center_x) / span_x;
        m[1] /= span_y; m[5] /= span_y;
        m[13] = (m[13] - center_y) / span_y;
    }
    /* Non-tight mode letterboxes the full canvas inside the allocated frame.
       Tight mode already remapped the cropped region onto the WHOLE window
       (which is sized to that crop): applying the letterbox viewport on top
       squashes the render into a sub-rect (e.g. 349px inside a 393px window)
       and leaves a green strip — pixel rounding makes that sub-rect differ
       from the window by construction, so it can never be skipped "when
       settled". */
    if (!tight_projection)
        apply_viewport_projection(projection);
    if (tight_projection) {
        /* Diagnostic: where does the drawn content actually end, compared
           with the frozen crop's edges (both in final window NDC)? */
        record_visible_state(projection);
        /* Geometry settlement: wait until the native resize has landed (or
           been rejected and rolled back) and stopped changing. */
        bool geom_same = bongo_cat_frame_equal(frame_, tight_geom_last_frame_) &&
            width_ == tight_geom_last_width_ &&
            height_ == tight_geom_last_height_;
        tight_geom_last_frame_ = frame_;
        tight_geom_last_width_ = width_;
        tight_geom_last_height_ = height_;
        tight_geom_settle_ = geom_same ? tight_geom_settle_ + 1 : 0;
        bool tight_settled = tight_geom_settle_ >= 3;
        if (tight_settled && visual_state_.visible) {
            /* Bounds were measured through the frame_-based remap, so margin
               inversion uses that same allocated frame. */
            float span_x = 1.0f + frame_.left + frame_.right;
            float span_y = 1.0f + frame_.top + frame_.bottom;
            float center_x = frame_.right - frame_.left;
            float center_y = frame_.top - frame_.bottom;
            /* Invert window-NDC content bounds into base-canvas margins. */
            auto compute_wanted = [&](float vmin_x, float vmax_x,
                    float vmin_y, float vmax_y, float out[4]) {
                float top_target = (center_y + vmax_y * span_y - 1.0f) * 0.5f;
                float bottom_target = -(center_y +
                    vmin_y * span_y + 1.0f) * 0.5f;
                float right_target = (center_x +
                    vmax_x * span_x - 1.0f) * 0.5f;
                float left_target = -(center_x +
                    vmin_x * span_x + 1.0f) * 0.5f;
                const float gx = 4.0f / std::max(1, width_);
                const float gy = 4.0f / std::max(1, height_);
                const float targets[4] = { top_target, bottom_target,
                    left_target, right_target };
                /* The static 2D overlay art (desk/keyboard) is part of the
                   visual content but invisible to the vertex envelope; the
                   tight crop must cover its bounds too or it gets sliced
                   (the reported clipped-keyboard bug). Art margins convert
                   the canvas-NDC rect the same way the inversion above does. */
                float covered[4] = { targets[0], targets[1],
                    targets[2], targets[3] };
                if (tight_overlay_valid_) {
                    float rx0 = tight_overlay_rect_[0];
                    float ry0 = tight_overlay_rect_[1];
                    float rx1 = tight_overlay_rect_[2];
                    float ry1 = tight_overlay_rect_[3];
                    if (mirror_) {
                        float swap = rx0;
                        rx0 = -rx1;
                        rx1 = -swap;
                    }
                    float art_top = (ry1 - 1.0f) * 0.5f;
                    float art_bottom = -(ry0 + 1.0f) * 0.5f;
                    float art_left = -(rx0 + 1.0f) * 0.5f;
                    float art_right = (rx1 - 1.0f) * 0.5f;
                    /* Margins grow outwards: a larger margin covers more.
                       The union takes the max on every edge. */
                    if (art_top > covered[0]) covered[0] = art_top;
                    if (art_bottom > covered[1]) covered[1] = art_bottom;
                    if (art_left > covered[2]) covered[2] = art_left;
                    if (art_right > covered[3]) covered[3] = art_right;
                }
                const float guards[4] = { gy, gy, gx, gx };
                const float spans[4] = { span_y, span_y, span_x, span_x };
                for (int e = 0; e < 4; ++e) {
                    float guard_m = guards[e] * spans[e] * 0.5f;
                    /* May be NEGATIVE: models often author the pet inside a
                       larger transparent canvas. Quantize to a fine 1/64
                       grid and round UP (keep a sliver of slack, never into
                       content). A coarser grid wasted up to a 1/16 margin
                       per edge — ~21 px of green on a 333 px canvas. */
                    out[e] = (float)(std::ceil(
                        (covered[e] + guard_m) * 64.0f) / 64.0f);
                }
                /* Honour the OS minimum window size. */
                const double min_sx =
                    160.0 / (double)tight_reference_content_width_;
                const double min_sy =
                    160.0 / (double)tight_reference_content_height_;
                if (1.0 + out[2] + out[3] < min_sx) {
                    float pad = (float)((min_sx -
                        (1.0 + out[2] + out[3])) * 0.5);
                    out[2] += pad; out[3] += pad;
                }
                if (1.0 + out[0] + out[1] < min_sy) {
                    float pad = (float)((min_sy -
                        (1.0 + out[0] + out[1])) * 0.5);
                    out[0] += pad; out[1] += pad;
                }
            };
            /* Phase 1 — OBSERVE: the window is intentionally held at the
               inherited (large) frame and we only accumulate the rendered
               bounds union. Bounds are recorded in WINDOW NDC, which shifts
               while the window shrinks, so inverting them mid-animation feeds
               the chase back into itself and locks at a wrong size (the
               permanent green-band bug). Observing with a still window yields
               true base-canvas margins; post-lock growth covers any pose not
               seen during this short window. */
            if (!tight_observed_) {
                if (tight_observe_frames_ == 0) {
                    tight_observe_bounds_[0] = visual_state_.visible_min_x;
                    tight_observe_bounds_[1] = visual_state_.visible_max_x;
                    tight_observe_bounds_[2] = visual_state_.visible_min_y;
                    tight_observe_bounds_[3] = visual_state_.visible_max_y;
                } else {
                    tight_observe_bounds_[0] = std::min(
                        tight_observe_bounds_[0], visual_state_.visible_min_x);
                    tight_observe_bounds_[1] = std::max(
                        tight_observe_bounds_[1], visual_state_.visible_max_x);
                    tight_observe_bounds_[2] = std::min(
                        tight_observe_bounds_[2], visual_state_.visible_min_y);
                    tight_observe_bounds_[3] = std::max(
                        tight_observe_bounds_[3], visual_state_.visible_max_y);
                }
                constexpr int tight_observe_frames_target = 45;
                if (++tight_observe_frames_ >= tight_observe_frames_target) {
                    float wanted[4];
                    compute_wanted(tight_observe_bounds_[0],
                        tight_observe_bounds_[1], tight_observe_bounds_[2],
                        tight_observe_bounds_[3], wanted);
                    required_frame_.top = wanted[0];
                    required_frame_.bottom = wanted[1];
                    required_frame_.left = wanted[2];
                    required_frame_.right = wanted[3];
                    tight_observed_ = true;
                    tight_geom_settle_ = 0;
                    SDL_LogInfo(SDL_LOG_CATEGORY_CUSTOM,
                        "[tight] observe done -> converging "
                        "(t=%.3f b=%.3f l=%.3f r=%.3f)",
                        wanted[0], wanted[1], wanted[2], wanted[3]);
                }
            } else {
                /* Window is (nearly) still now, so per-frame inversion is
                   trustworthy. */
                float wanted[4];
                compute_wanted(visual_state_.visible_min_x,
                    visual_state_.visible_max_x,
                    visual_state_.visible_min_y,
                    visual_state_.visible_max_y, wanted);
                constexpr float grid_eps = 1.0f / 64.0f;
                if (!tight_locked_) {
                    /* Phase 2 — CONVERGE: the wanted target from the observe
                       phase is frozen; wait for the animated frame to arrive
                       and hold still, then lock. */
                    bool arrived = true;
                    const float frame_edges[4] = { frame_.top, frame_.bottom,
                        frame_.left, frame_.right };
                    const float target_edges[4] = { required_frame_.top,
                        required_frame_.bottom, required_frame_.left,
                        required_frame_.right };
                    for (int e = 0; e < 4; ++e) {
                        if (std::fabs(frame_edges[e] - target_edges[e])
                                > grid_eps)
                            arrived = false;
                    }
                    if (arrived) {
                        tight_locked_ = true;
                        SDL_LogInfo(SDL_LOG_CATEGORY_CUSTOM,
                            "[tight] window frame locked "
                            "(t=%.3f b=%.3f l=%.3f r=%.3f)",
                            target_edges[0], target_edges[1],
                            target_edges[2], target_edges[3]);
                    }
                }
                /* Phase 3 — TRACK, lazily. Corrections run while the
                   geometry is settled (gated above) and the inversion uses
                   the same frame_ the visible bounds were measured with, so
                   the loop is contractive rather than a chase. Growth is
                   immediate: a pose that would clip must win right away.
                   Shrink waits for a dwell period — the user wants a still
                   window, so transient slack is only reclaimed after the
                   content has stayed smaller for a while. */
                {
                    const float required_edges[4] = { required_frame_.top,
                        required_frame_.bottom, required_frame_.left,
                        required_frame_.right };
                    float next_edges[4] = { required_edges[0],
                        required_edges[1], required_edges[2],
                        required_edges[3] };
                    bool any_shrink = false;
                    bool any_change = false;
                    uint64_t now_ticks = SDL_GetTicksNS();
                    for (int e = 0; e < 4; ++e) {
                        if (wanted[e] > required_edges[e] + grid_eps) {
                            next_edges[e] = wanted[e];
                            any_change = true;
                        } else if (wanted[e] < required_edges[e] - grid_eps) {
                            any_shrink = true;
                            if (tight_shrink_due_ns_ != 0 &&
                                now_ticks >= tight_shrink_due_ns_) {
                                next_edges[e] = wanted[e];
                                any_change = true;
                            }
                        }
                    }
                    if (!any_shrink) tight_shrink_due_ns_ = 0;
                    else if (tight_shrink_due_ns_ == 0)
                        tight_shrink_due_ns_ = now_ticks +
                            tight_shrink_dwell_ns;
                    if (any_change) {
                        required_frame_.top = next_edges[0];
                        required_frame_.bottom = next_edges[1];
                        required_frame_.left = next_edges[2];
                        required_frame_.right = next_edges[3];
                    /* Rebase this frame's remap onto the new crop:
                       R_new ∘ R_old⁻¹ (both affine, applied in place). */
                    float old_span_x = span_x, old_span_y = span_y;
                    float old_center_x = center_x, old_center_y = center_y;
                    span_x = 1.0f + required_frame_.left +
                        required_frame_.right;
                    span_y = 1.0f + required_frame_.top +
                        required_frame_.bottom;
                    center_x = required_frame_.right -
                        required_frame_.left;
                    center_y = required_frame_.top -
                        required_frame_.bottom;
                    float scale_x = old_span_x / span_x;
                    float scale_y = old_span_y / span_y;
                    float *m = projection.GetArray();
                    m[0] *= scale_x; m[4] *= scale_x;
                    m[12] = m[12] * scale_x +
                        (old_center_x - center_x) / span_x;
                    m[1] *= scale_y; m[5] *= scale_y;
                    m[13] = m[13] * scale_y +
                        (old_center_y - center_y) / span_y;
                    }
                }
            }
        }
        static int tight_log_frames;
        if (visual_state_.visible && tight_log_frames++ % 60 == 0) {
            SDL_LogInfo(SDL_LOG_CATEGORY_CUSTOM,
                "[tight] visible x=%.3f..%.3f y=%.3f..%.3f "
                "crop x=%.3f..%.3f y=%.3f..%.3f "
                "alloc x=%.3f..%.3f y=%.3f..%.3f vp=%d,%d,%d,%d",
                visual_state_.visible_min_x, visual_state_.visible_max_x,
                visual_state_.visible_min_y, visual_state_.visible_max_y,
                -1.0f - 2.0f * required_frame_.left,
                1.0f + 2.0f * required_frame_.right,
                -1.0f - 2.0f * required_frame_.bottom,
                1.0f + 2.0f * required_frame_.top,
                frame_.left, frame_.right, frame_.top, frame_.bottom,
                viewport_x_, viewport_y_, viewport_width_, viewport_height_);
        }
    }
    visual_state_ = BongoCatLive2DVisualState{};
    visual_state_.fit_scale = frame_fit_scale_;
    visual_state_.fitted = frame_fit_scale_ < 0.9999f;
    visual_state_.mver_projection = render_options_.mver_projection;
    visual_projection_.SetMatrix(projection.GetArray());
    visual_state_cached_ = false;
    visual_state_ready_ = true;
    renderer->SetMvpMatrix(&projection);
    renderer->SetModelColor(1.0f, 1.0f, 1.0f, _model->GetModelOpacity());
    {
        DrawFrame frame(manager);
        renderer->DrawModel();
    }
    if (trim_offscreen_pool_) {
        // The first completed frame establishes how many targets this model
        // needs. Drop unused targets retained by a previously loaded model.
        manager->ReleaseStaleRenderTextures();
        trim_offscreen_pool_ = false;
    }
    if (resources_changed) {
        // Cubism creates each clipping manager only when the model uses that
        // mask type. Its count getters dereference those optional managers.
        const int drawable_count = _model->IsUsingMasking()
            ? renderer->GetDrawableRenderTextureCount() : 0;
        const int offscreen_count = _model->IsUsingMaskingForOffscreen()
            ? renderer->GetOffscreenRenderTextureCount() : 0;
        double mask_mib = drawable_count * bongo_cat_model_texture_mib(
            drawable_masks_.size.width, drawable_masks_.size.height, false) +
            offscreen_count * bongo_cat_model_texture_mib(
                offscreen_masks_.size.width, offscreen_masks_.size.height, false);
        bongo_cat_resource_trace_render(mask_mib,
            (unsigned)manager->GetOffscreenRenderTargetListSize(), width_, height_);
        if (first_frame) bongo_cat_model_memory_log("renderer-first-frame",
            "window=%dx%d viewport=%dx%d drawable_masks=%dx%dx%d "
            "offscreen_masks=%dx%dx%d mask_rgba8_est_mib=%.1f pool_targets=%u",
            width_, height_, viewport_width_, viewport_height_,
            drawable_masks_.size.width, drawable_masks_.size.height, drawable_count,
            offscreen_masks_.size.width, offscreen_masks_.size.height, offscreen_count,
            mask_mib, (unsigned)manager->GetOffscreenRenderTargetListSize());
    }
}

void NativeModel::release_renderer() {
    DeleteRenderer();
#ifdef CSM_TARGET_MAC_GL
    core_buffers_.release();
#endif
    renderer_width_ = 0;
    renderer_height_ = 0;
    mask_texture_limit_ = 0;
    drawable_masks_ = {};
    offscreen_masks_ = {};
    mask_update_failed_ = false;
    mask_last_width_ = mask_last_height_ = 0;
    visual_state_ready_ = false;
    trim_offscreen_pool_ = true;
}

void NativeModel::release_render_resources() {
    release_textures();
    release_renderer();
}

bool NativeModel::create_renderer(BongoCatError *error) {
    if (!SDL_GL_GetCurrentContext()) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "Cannot create the Live2D renderer without an OpenGL context");
        return false;
    }
    if (!bongo_cat_gl_clear_errors()) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_CUBISM,
            "Cannot clear the OpenGL error state before creating the Live2D renderer");
        return false;
    }
#ifdef CSM_TARGET_MAC_GL
    CoreProfileBinding binding(core_buffers_);
#endif
    const int buffer_count = prepare_mask_layout();
    TargetBindings bindings;
    CreateRenderer((Csm::csmUint32)width_, (Csm::csmUint32)height_,
        buffer_count);
    auto *renderer = GetRenderer<Csm::Rendering::CubismRenderer_OpenGLES2>();
    GLenum renderer_error = glGetError();
    if (renderer && renderer_error == GL_NO_ERROR) {
        GLint texture_limit = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &texture_limit);
        renderer_error = glGetError();
        if (renderer_error != GL_NO_ERROR || texture_limit <= 0) {
            bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
                "Cannot query the Live2D clipping texture size limit");
            release_renderer();
            return false;
        }
        mask_texture_limit_ = (int)texture_limit;
        bind_textures();
        if (!update_mask_buffers()) {
            bongo_cat_error_set(error, BONGO_CAT_ERROR_CUBISM,
                "Cannot allocate full-resolution Live2D clipping masks");
            release_renderer();
            return false;
        }
    }
    if (renderer_error == GL_NO_ERROR) renderer_error = glGetError();
    if (renderer && renderer_error == GL_NO_ERROR) return true;
    bongo_cat_error_set(error, BONGO_CAT_ERROR_CUBISM,
        "Cannot create the Live2D renderer (OpenGL 0x%x)",
        (unsigned)renderer_error);
    release_renderer();
    return false;
}

} // namespace bongo_cat
