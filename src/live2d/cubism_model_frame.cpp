#include "cubism_model.hpp"
#include "model_frame_policy.h"

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_timer.h>

#include <algorithm>
#include <cmath>

namespace bongo_cat {
namespace {
/* Prefix sums of the existing conservative alpha mask make triangle UV-box
   queries constant time. Only used while loading; no texture readback. */
struct AlphaCoverage {
    int width = 0, height = 0;
    std::vector<unsigned> sums;

    explicit AlphaCoverage(const BongoCatImageAlphaMask *mask) {
        if (!mask || mask->width <= 0 || mask->height <= 0) return;
        width = mask->width;
        height = mask->height;
        sums.resize((size_t)(width + 1) * (height + 1));
        for (int y = 0; y < height; ++y) {
            unsigned row = 0;
            for (int x = 0; x < width; ++x) {
                row += mask->pixels[(size_t)y * width + x] > 0;
                sums[(size_t)(y + 1) * (width + 1) + x + 1] =
                    sums[(size_t)y * (width + 1) + x + 1] + row;
            }
        }
    }

    bool visible(float min_u, float min_v, float max_u, float max_v) const {
        if (sums.empty() || !std::isfinite(min_u) || !std::isfinite(min_v) ||
            !std::isfinite(max_u) || !std::isfinite(max_v)) return true;
        auto cell = [](float value, int size) {
            return (int)(std::max(0.0f, std::min(1.0f, value)) * (size - 1));
        };
        /* Include neighbouring cells for texture filtering and UV rounding.
           A UV bounding box may include transparent corners; false positives
           cost a little space, whereas false negatives would clip thin parts. */
        int x0 = std::max(0, cell(min_u, width) - 1);
        int y0 = std::max(0, cell(min_v, height) - 1);
        int x1 = std::min(width, cell(max_u, width) + 3);
        int y1 = std::min(height, cell(max_v, height) + 3);
        size_t stride = (size_t)width + 1;
        return sums[(size_t)y1 * stride + x1] + sums[(size_t)y0 * stride + x0] >
            sums[(size_t)y0 * stride + x1] + sums[(size_t)y1 * stride + x0];
    }
};
}

void NativeModel::prepare_frame_bounds() {
    frame_drawables_.clear();
    if (!_model) return;
    std::vector<AlphaCoverage> coverage;
    coverage.reserve(texture_count());
    for (size_t i = 0; i < texture_count(); ++i)
        coverage.emplace_back(texture_alpha((int)i));
    frame_drawables_.resize((size_t)_model->GetDrawableCount());
    for (int i = 0; i < _model->GetDrawableCount(); ++i) {
        int count = _model->GetDrawableVertexCount(i);
        const auto *indices = _model->GetDrawableVertexIndices(i);
        const auto *uv = _model->GetDrawableVertexUvs(i);
        if (count <= 0 || !indices || !uv) continue;
        std::vector<unsigned char> used((size_t)count, 0);
        int texture = _model->GetDrawableTextureIndex(i);
        const AlphaCoverage *alpha = texture >= 0 && (size_t)texture < coverage.size()
            ? &coverage[(size_t)texture] : nullptr;
        int index_count = _model->GetDrawableVertexIndexCount(i);
        for (int j = 0; j + 2 < index_count; j += 3) {
            unsigned short a = indices[j], b = indices[j + 1], c = indices[j + 2];
            if (a >= count || b >= count || c >= count) continue;
            if (alpha && !alpha->visible(
                std::min({uv[a].X, uv[b].X, uv[c].X}),
                std::min({uv[a].Y, uv[b].Y, uv[c].Y}),
                std::max({uv[a].X, uv[b].X, uv[c].X}),
                std::max({uv[a].Y, uv[b].Y, uv[c].Y}))) continue;
            used[a] = used[b] = used[c] = 1;
        }
        auto &vertices = frame_drawables_[(size_t)i].vertices;
        for (int j = 0; j < count; ++j)
            if (used[(size_t)j]) vertices.push_back((unsigned short)j);
    }
}

bool NativeModel::measure_frame(BongoCatLive2DFrame *required) {
    if (!_model || !required) return false;
    ModelBounds envelope;
    auto include = [](ModelBounds &bounds, float x, float y) {
        if (!std::isfinite(x) || !std::isfinite(y)) return;
        if (!bounds.valid) {
            bounds = {x, y, x, y, true};
            return;
        }
        bounds.min_x = std::min(bounds.min_x, x);
        bounds.min_y = std::min(bounds.min_y, y);
        bounds.max_x = std::max(bounds.max_x, x);
        bounds.max_y = std::max(bounds.max_y, y);
    };
    if (_model->GetModelOpacity() > 0.001f) {
        if (frame_drawables_.size() != (size_t)_model->GetDrawableCount()) {
            for (int i = 0; i < _model->GetDrawableCount(); ++i) {
                if (!_model->GetDrawableDynamicFlagIsVisible(i) ||
                    _model->GetDrawableOpacity(i) <= 0.001f) continue;
                const float *positions = _model->GetDrawableVertices(i);
                if (!positions) continue;
                for (int j = 0; j < _model->GetDrawableVertexCount(i); ++j)
                    include(envelope, positions[j * 2], positions[j * 2 + 1]);
            }
        }
        for (size_t i = 0; i < frame_drawables_.size(); ++i) {
            if (!_model->GetDrawableDynamicFlagIsVisible((int)i) ||
                _model->GetDrawableOpacity((int)i) <= 0.001f) continue;
            auto &drawable = frame_drawables_[i];
            if (drawable.dirty) {
                drawable.bounds = {};
                const float *positions = _model->GetDrawableVertices((int)i);
                if (positions)
                    for (unsigned short vertex : drawable.vertices)
                        include(drawable.bounds, positions[vertex * 2],
                            positions[vertex * 2 + 1]);
                drawable.dirty = false;
            }
            if (drawable.bounds.valid) {
                include(envelope, drawable.bounds.min_x, drawable.bounds.min_y);
                include(envelope, drawable.bounds.max_x, drawable.bounds.max_y);
            }
        }
    }
    /* Use the unpadded content aspect. The fitted viewport must never feed
       back into boundary measurement or an extreme motion could grow forever. */
    int content_width = tight_frame_ && tight_reference_content_width_ > 0 ?
        tight_reference_content_width_ : (int)std::max(1.0, std::round(width_ /
        (1.0 + frame_.left + frame_.right)));
    int content_height = tight_frame_ && tight_reference_content_height_ > 0 ?
        tight_reference_content_height_ : (int)std::max(1.0, std::round(height_ /
        (1.0 + frame_.top + frame_.bottom)));
    if (envelope.valid) {
        Csm::CubismMatrix44 projection;
        build_projection(projection, content_width, content_height);
        float x0 = projection.TransformX(envelope.min_x);
        float x1 = projection.TransformX(envelope.max_x);
        float y0 = projection.TransformY(envelope.min_y);
        float y1 = projection.TransformY(envelope.max_y);
        /* Trigger slightly before contact, including raster/filter coverage. */
        float guard_x = 4.0f / content_width, guard_y = 4.0f / content_height;
        float min_x = std::min(x0, x1) - guard_x;
        float min_y = std::min(y0, y1) - guard_y;
        float max_x = std::max(x0, x1) + guard_x;
        float max_y = std::max(y0, y1) + guard_y;
        if (!tight_frame_) {
            required_frame_ = bongo_cat_frame_observe(required_frame_,
                min_x, min_y, max_x, max_y);
        }
        /* Tight mode: required_frame_ is owned by the draw pass, which knows
           the true rendered bounds; the vertex envelope over-counts for some
           models (invisible oversized drawables) and would crop wrongly. */
    }
    /* Tight mode: ease the ALLOCATED frame toward the target at constant
       edge speeds instead of jumping a whole bucket at once. The runtime
       resizes its window to whatever frame we return here, so returning an
       intermediate frame animates the geometry linearly. The animation
       always starts from frame_ (the truly allocated frame): if the window
       manager rejects a resize, frame_ never advances and the next advance
       simply tries again. Speeds are asymmetric: growth is quick (content
       must not be clipped long), shrinking is slow and gentle. Geometry is
       published at ~30 Hz to avoid hammering the window manager. */
    BongoCatLive2DFrame output = required_frame_;
    if (tight_frame_) {
        constexpr float grow_speed_px_s = 1800.0f;
        constexpr float shrink_speed_px_s = 420.0f;
        constexpr uint64_t publish_interval_ns = 33ULL * 1000000ULL;
        uint64_t now = SDL_GetTicksNS();
        output = frame_;
        if (tight_anim_last_ns_ != 0 &&
            now - tight_anim_last_ns_ < publish_interval_ns) {
            /* Throttle window: hold this tick, keep the established frame. */
        } else {
            float dt = tight_anim_last_ns_ == 0
                ? 1.0f / 60.0f
                : (float)((double)(now - tight_anim_last_ns_) / 1.0e9);
            tight_anim_last_ns_ = now;
            dt = std::max(0.001f, std::min(0.05f, dt));
            auto approach = [](float current, float target, float step) {
                float d = target - current;
                if (std::fabs(d) <= step) return target;
                return current + (d > 0.0f ? step : -step);
            };
            auto advance = [&](float current, float target, int ref) {
                if (current == target) return target;
                float speed = target > current ? grow_speed_px_s
                                               : shrink_speed_px_s;
                float step = speed * dt / std::max(1, ref);
                return approach(current, target, step);
            };
            output.left = advance(frame_.left, required_frame_.left,
                tight_reference_content_width_);
            output.right = advance(frame_.right, required_frame_.right,
                tight_reference_content_width_);
            output.top = advance(frame_.top, required_frame_.top,
                tight_reference_content_height_);
            output.bottom = advance(frame_.bottom, required_frame_.bottom,
                tight_reference_content_height_);
        }
    } else {
        tight_anim_last_ns_ = 0;
    }
    *required = output;
    update_viewport();
    return true;
}

} // namespace bongo_cat
