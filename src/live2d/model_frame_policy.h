#ifndef BONGO_CAT_MODEL_FRAME_POLICY_H
#define BONGO_CAT_MODEL_FRAME_POLICY_H

#include "bongo_cat/model.h"
#include <math.h>

/* Shared by the native window and Cubism. Margins are canvas fractions, not
   framebuffer fractions. Growth in eighths amortizes surface reallocations.
   Negative margins crop the canvas: the frame system uses them for the tight
   window mode, where the window shrinks to the pet's visible envelope. */
static inline bool bongo_cat_frame_valid(BongoCatLive2DFrame f) {
    return isfinite(f.left) && isfinite(f.top) && isfinite(f.right) &&
        isfinite(f.bottom) && f.left > -1.0f && f.top > -1.0f &&
        f.right > -1.0f && f.bottom > -1.0f &&
        1.0f + f.left + f.right >= 0.01f &&
        1.0f + f.top + f.bottom >= 0.01f;
}

static inline bool bongo_cat_frame_equal(BongoCatLive2DFrame a,
    BongoCatLive2DFrame b) {
    return a.left == b.left && a.top == b.top &&
        a.right == b.right && a.bottom == b.bottom;
}

static inline float bongo_cat_frame_margin(float previous, double overflow) {
    if (!isfinite(overflow) || overflow <= previous) return previous;
    /* Half a bucket of headroom, rounded outwards. No shrinking until unload. */
    return (float)(ceil((overflow + 0.0625) * 8.0) / 8.0);
}

static inline BongoCatLive2DFrame bongo_cat_frame_tight(
    float min_x, float min_y, float max_x, float max_y) {
    BongoCatLive2DFrame frame = {0, 0, 0, 0};
    if (!isfinite(min_x) || !isfinite(min_y) || !isfinite(max_x) ||
        !isfinite(max_y) || min_x > max_x || min_y > max_y)
        return frame;
    /* Negative margins crop the base canvas down to the envelope. */
    frame.left = (-1.0f - min_x) * 0.5f;
    frame.right = (max_x - 1.0f) * 0.5f;
    frame.top = (max_y - 1.0f) * 0.5f;
    frame.bottom = (-1.0f - min_y) * 0.5f;
    if (frame.left < -0.975f) frame.left = -0.975f;
    if (frame.right < -0.975f) frame.right = -0.975f;
    if (frame.top < -0.975f) frame.top = -0.975f;
    if (frame.bottom < -0.975f) frame.bottom = -0.975f;
    return frame;
}

static inline BongoCatLive2DFrame bongo_cat_frame_observe(
    BongoCatLive2DFrame previous, float min_x, float min_y,
    float max_x, float max_y) {
    if (!isfinite(min_x) || !isfinite(min_y) || !isfinite(max_x) ||
        !isfinite(max_y) || min_x > max_x || min_y > max_y) return previous;
    previous.left = bongo_cat_frame_margin(previous.left, (-1.0 - min_x) * 0.5);
    previous.right = bongo_cat_frame_margin(previous.right, (max_x - 1.0) * 0.5);
    previous.top = bongo_cat_frame_margin(previous.top, (max_y - 1.0) * 0.5);
    previous.bottom = bongo_cat_frame_margin(previous.bottom, (-1.0 - min_y) * 0.5);
    return previous;
}

static inline double bongo_cat_frame_area(BongoCatLive2DFrame f) {
    return (1.0 + f.left + f.right) * (1.0 + f.top + f.bottom);
}

static inline BongoCatLive2DFrame bongo_cat_frame_mix(
    BongoCatLive2DFrame a, BongoCatLive2DFrame b, double t) {
    /* Bidirectional: the tight mode shrinks (crops) from the current frame,
       while observe-mode targets only ever grow. */
    BongoCatLive2DFrame f;
    f.left = (float)(a.left + (b.left - a.left) * t);
    f.top = (float)(a.top + (b.top - a.top) * t);
    f.right = (float)(a.right + (b.right - a.right) * t);
    f.bottom = (float)(a.bottom + (b.bottom - a.bottom) * t);
    return f;
}

/* Budgets are relative to the content canvas. Preserve an existing allocation
   if a DPI/display change makes it exceed a budget; never enlarge it further.
   bucket_div sets the resize grid as fractions of the canvas (8 = 1/8 buckets
   for observe mode, 32 = fine ~pixel buckets for tight cropping, where a
   single 1/8 step would visibly squash the pet by 12.5%). */
static inline BongoCatLive2DFrame bongo_cat_frame_limit_ex(
    BongoCatLive2DFrame current, BongoCatLive2DFrame required,
    double area_limit, double width_limit, double height_limit,
    double bucket_div) {
    if (!bongo_cat_frame_valid(current) || !bongo_cat_frame_valid(required) ||
        !isfinite(area_limit) || !isfinite(width_limit) || !isfinite(height_limit) ||
        bucket_div <= 0.0)
        return current;
    area_limit = fmax(area_limit, bongo_cat_frame_area(current));
    width_limit = fmax(width_limit, 1.0 + current.left + current.right);
    height_limit = fmax(height_limit, 1.0 + current.top + current.bottom);
    double low = 0.0, high = 1.0;
    for (int i = 0; i < 25; ++i) {
        double t = i == 0 ? 1.0 : (low + high) * 0.5;
        BongoCatLive2DFrame f = bongo_cat_frame_mix(current, required, t);
        if (bongo_cat_frame_area(f) <= area_limit &&
            1.0 + f.left + f.right <= width_limit &&
            1.0 + f.top + f.bottom <= height_limit) {
            low = t;
            if (t == 1.0) break;
        } else high = t;
    }
    BongoCatLive2DFrame f = bongo_cat_frame_mix(current, required, low);
    /* Round whole buckets away from the target: growth rounds down, crop
       rounds up. Cropping past the envelope would clip pet pixels. */
    float *edges[] = {&f.left, &f.right, &f.top, &f.bottom};
    const float base[] = {current.left, current.right,
        current.top, current.bottom};
    for (int i = 0; i < 4; ++i) {
        double buckets = ((double)*edges[i] - base[i]) * bucket_div;
        *edges[i] = base[i] + (float)((buckets >= 0.0 ?
            floor(buckets) : ceil(buckets * bucket_div) / bucket_div) / bucket_div);
    }
    /* Spend the rounding remainder now. Otherwise the same unchanged request
       can creep outward over several frames, reallocating a surface each time.
       Sub-bucket remainders are spent exactly: dropping them made the tight
       window mode shrink a little on every toggle. */
    const float targets[] = {required.left, required.right, required.top, required.bottom};
    const double bucket_size = 1.0 / bucket_div;
    for (int i = 0; i < 4; ++i) {
        double horizontal = 1.0 + f.left + f.right;
        double vertical = 1.0 + f.top + f.bottom;
        double room = i < 2 ? fmin(width_limit, area_limit / vertical) - horizontal :
            fmin(height_limit, area_limit / horizontal) - vertical;
        double spend = fmin(room, (double)targets[i] - *edges[i]);
        if (spend <= 0.0) continue;
        *edges[i] += (float)(spend >= bucket_size ?
            floor(spend * bucket_div) / bucket_div : spend);
    }
    return f;
}

static inline BongoCatLive2DFrame bongo_cat_frame_limit(
    BongoCatLive2DFrame current, BongoCatLive2DFrame required,
    double area_limit, double width_limit, double height_limit) {
    return bongo_cat_frame_limit_ex(current, required, area_limit,
        width_limit, height_limit, 8.0);
}

typedef struct BongoCatFrameViewport {
    int x, y, width, height;
    float scale;
} BongoCatFrameViewport;

static inline BongoCatFrameViewport bongo_cat_frame_viewport(
    BongoCatLive2DFrame allocated, BongoCatLive2DFrame required,
    int width, int height, bool flip) {
    BongoCatFrameViewport v = {0, 0, width, height, 1.0f};
    if (width <= 0 || height <= 0 || !bongo_cat_frame_valid(allocated) ||
        !bongo_cat_frame_valid(required)) return v;
    required = bongo_cat_frame_mix(allocated, required, 1.0);
    double horizontal = 1.0 + allocated.left + allocated.right;
    double vertical = 1.0 + allocated.top + allocated.bottom;
    double scale = fmin(horizontal / (1.0 + required.left + required.right),
        vertical / (1.0 + required.top + required.bottom));
    double cw = width / horizontal * scale, ch = height / vertical * scale;
    double bottom = flip ? required.top : required.bottom;
    double top = flip ? required.bottom : required.top;
    v.width = (int)fmax(1.0, scale >= 1.0 ? round(cw) : floor(cw));
    v.height = (int)fmax(1.0, scale >= 1.0 ? round(ch) : floor(ch));
    v.x = (int)lround(fmax(0.0, fmin(width - v.width,
        (width - v.width * (1.0 + required.right - required.left)) * 0.5)));
    v.y = (int)lround(fmax(0.0, fmin(height - v.height,
        (height - v.height * (1.0 + top - bottom)) * 0.5)));
    v.scale = (float)scale;
    return v;
}

#endif
