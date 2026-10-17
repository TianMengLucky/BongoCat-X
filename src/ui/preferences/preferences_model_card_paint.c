#include "preferences_model_card_paint.h"
#include "ui_icons.h"
#include "ui_paint.h"

#include <math.h>

#define MODEL_PROGRESS_ARC_SEGMENTS 16
#define MODEL_PROGRESS_POINT_CAP 80

struct nk_rect bongo_cat_preferences_model_card_outline_bounds(
    struct nk_rect bounds, float thickness) {
    float inset = thickness * .5f;
    return nk_rect(bounds.x + inset, bounds.y + inset,
        NK_MAX(1.0f, bounds.w - thickness),
        NK_MAX(1.0f, bounds.h - thickness));
}

static int progress_point(float *points, int count, float x, float y) {
    if (count >= MODEL_PROGRESS_POINT_CAP) return count;
    points[count * 2] = x;
    points[count * 2 + 1] = y;
    return count + 1;
}

static int rounded_outline_path(struct nk_rect bounds, float rounding,
    float *points) {
    const float pi = 3.14159265358979323846f;
    float radius = NK_CLAMP(0.0f, rounding,
        NK_MIN(bounds.w, bounds.h) * .5f);
    float centers_x[] = {bounds.x + bounds.w - radius,
        bounds.x + bounds.w - radius, bounds.x + radius, bounds.x + radius};
    float centers_y[] = {bounds.y + radius, bounds.y + bounds.h - radius,
        bounds.y + bounds.h - radius, bounds.y + radius};
    float starts[] = {-pi * .5f, 0.0f, pi * .5f, pi};
    int count = 0;
    count = progress_point(points, count, bounds.x + radius, bounds.y);
    for (int corner = 0; corner < 4; ++corner) {
        for (int i = 0; i <= MODEL_PROGRESS_ARC_SEGMENTS; ++i) {
            float angle = starts[corner] + pi * .5f * i /
                MODEL_PROGRESS_ARC_SEGMENTS;
            count = progress_point(points, count,
                centers_x[corner] + cosf(angle) * radius,
                centers_y[corner] + sinf(angle) * radius);
        }
    }
    count = progress_point(points, count, points[0], points[1]);
    return count;
}

void bongo_cat_preferences_model_card_draw_progress(
    struct nk_command_buffer *canvas, struct nk_rect bounds, float rounding,
    float thickness, float progress, struct nk_color color) {
    progress = NK_CLAMP(0.0f, progress, 1.0f);
    if (progress <= 0.001f) return;
    if (progress >= .999f) {
        nk_stroke_rect(canvas, bounds, rounding, thickness, color);
        return;
    }
    float path[MODEL_PROGRESS_POINT_CAP * 2];
    int path_count = rounded_outline_path(bounds, rounding, path);
    if (path_count < 2) return;
    float lengths[MODEL_PROGRESS_POINT_CAP];
    lengths[0] = 0.0f;
    for (int i = 1; i < path_count; ++i) {
        float dx = path[i * 2] - path[(i - 1) * 2];
        float dy = path[i * 2 + 1] - path[(i - 1) * 2 + 1];
        lengths[i] = lengths[i - 1] + sqrtf(dx * dx + dy * dy);
    }
    float target = lengths[path_count - 1] * progress;
    float partial[MODEL_PROGRESS_POINT_CAP * 2];
    int partial_count = progress_point(partial, 0, path[0], path[1]);
    for (int i = 1; i < path_count; ++i) {
        if (target >= lengths[i]) {
            partial_count = progress_point(partial, partial_count,
                path[i * 2], path[i * 2 + 1]);
            continue;
        }
        float segment = lengths[i] - lengths[i - 1];
        float amount = segment > 0.0f ?
            (target - lengths[i - 1]) / segment : 0.0f;
        partial_count = progress_point(partial, partial_count,
            path[(i - 1) * 2] +
                (path[i * 2] - path[(i - 1) * 2]) * amount,
            path[(i - 1) * 2 + 1] +
                (path[i * 2 + 1] - path[(i - 1) * 2 + 1]) * amount);
        break;
    }
    if (partial_count >= 2)
        nk_stroke_polyline(canvas, partial, partial_count, thickness, color);
}

void bongo_cat_preferences_model_card_draw_selection_badge(
    BongoCatPreferences *value, struct nk_command_buffer *canvas,
    struct nk_rect preview, BongoCatUIPalette palette, float amount) {
    if (amount <= .001f) return;
    float size = 25.0f * (.85f + .15f * amount);
    struct nk_rect badge = nk_rect(preview.x + 8,
        preview.y + 8, size, size);
    nk_fill_circle(canvas, badge,
        bongo_cat_ui_color_alpha(palette.accent, amount));
    bongo_cat_preferences_icon_draw(value, canvas, BONGO_CAT_UI_ICON_CHECK,
        nk_rect(badge.x + 5, badge.y + 5, badge.w - 10, badge.h - 10),
        bongo_cat_ui_color_alpha(palette.on_accent, amount));
}
