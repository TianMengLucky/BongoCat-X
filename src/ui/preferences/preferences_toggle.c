#include "preferences_controls.h"
#include "ui_animation.h"
#include "ui_backend.h"
#include "ui_catime.h"
#include "ui_paint.h"
#include "bongo_cat/config.h"

#include <stdio.h>

static bool toggle_draw(struct nk_context *context, const char *id,
    bool *value, struct nk_rect track, bool available) {
    const float effect_margin = 18.0f;
    struct nk_rect interaction = nk_rect(track.x, track.y,
        track.w + effect_margin, track.h);
    bool hover = available &&
        nk_input_is_mouse_hovering_rect(&context->input, interaction);
    bool changed = hover && nk_input_is_mouse_click_in_rect(&context->input,
        NK_BUTTON_LEFT, interaction);
    if (changed) *value = !*value;
    float progress = bongo_cat_ui_animate_eased(context, id,
        *value ? 1.0f : 0.0f, 250.0f, BONGO_CAT_UI_EASE_SPRING);
    char hover_id[80];
    snprintf(hover_id, sizeof(hover_id), "toggle-hover-%s", id);
    float hover_amount = bongo_cat_ui_animate_eased(context, hover_id,
        hover ? 1.0f : 0.0f, 250.0f, BONGO_CAT_UI_EASE_STANDARD);
    float scale = 1.0f + .05f * hover_amount;
    track = nk_rect(track.x - track.w * (scale - 1.0f) * .5f,
        track.y - track.h * (scale - 1.0f) * .5f,
        track.w * scale, track.h * scale);
    BongoCatUIPalette p = bongo_cat_ui_palette(
        bongo_cat_ui_dark(context));
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    float state_amount = NK_CLAMP(0.0f, progress, 1.0f);
    if (state_amount > 0.0f && p.effects)
        bongo_cat_ui_paint_shadow(context, track, 12, 0, 2, 8, 0,
            nk_rgba(p.accent.r, p.accent.g, p.accent.b,
            (nk_byte)(89 * state_amount)));
    nk_fill_rect(canvas, track, 12,
        bongo_cat_ui_color_mix(p.field, p.accent, state_amount));
    nk_stroke_rect(canvas, track, 12, 1.0f, bongo_cat_ui_color_mix(
        p.border_subtle, p.accent, state_amount));
    float knob_size = 18.0f * scale;
    float knob_x = track.x + 3.0f * scale +
        (track.w - 6.0f * scale - knob_size) * progress;
    struct nk_rect knob = nk_rect(knob_x,
        track.y + (track.h - knob_size) * .5f, knob_size, knob_size);
    if (p.effects) bongo_cat_ui_paint_shadow(context, knob,
        knob_size * .5f, 0, 2, 5, 0, nk_rgba(0, 0, 0, 51));
    nk_fill_circle(canvas, knob,
        available ? nk_rgb(255, 255, 255) : nk_rgb(245, 248, 252));
    if (hover) bongo_cat_ui_cursor_hover_rect(context, interaction,
        BONGO_CAT_UI_CURSOR_POINTER);
    return changed;
}

static bool toggle_at(struct nk_context *context, const char *id,
    bool *value, struct nk_rect cell, bool available) {
    const float effect_margin = 18.0f;
    struct nk_rect track = nk_rect(cell.x + cell.w - 46 - effect_margin,
        cell.y + (cell.h - 24) * .5f, 46, 24);
    return toggle_draw(context, id, value, track, available);
}

bool bongo_cat_pref_control_toggle(struct nk_context *context,
    const char *id, bool *value) {
    return bongo_cat_pref_control_toggle_available(context, id, value, true);
}

bool bongo_cat_pref_control_toggle_available(struct nk_context *context,
    const char *id, bool *value, bool available) {
    struct nk_rect cell;
    if (nk_widget(&cell, context) == NK_WIDGET_INVALID) return false;
    return toggle_at(context, id, value, cell, available);
}

bool bongo_cat_pref_control_toggle_rect(struct nk_context *context,
    const char *id, bool *value, struct nk_rect cell, bool available) {
    return toggle_at(context, id, value, cell, available);
}

/* Color chip shown in the row while the solid background is enabled. */
static void draw_color_chip(struct nk_context *context, struct nk_rect cell,
    uint32_t rgb) {
    struct nk_rect chip = nk_rect(cell.x + cell.w - 64.0f - 7.0f - 26.0f,
        cell.y + (cell.h - 24.0f) * .5f, 26.0f, 24.0f);
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    nk_fill_rect(canvas, chip, 6, nk_rgb((rgb >> 16) & 255,
        (rgb >> 8) & 255, rgb & 255));
    nk_stroke_rect(canvas, chip, 6, 1.0f, p.border);
    if (nk_input_is_mouse_hovering_rect(&context->input, chip))
        bongo_cat_ui_cursor_hover_rect(context, chip,
            BONGO_CAT_UI_CURSOR_POINTER);
}

bool bongo_cat_pref_control_obs_background(struct nk_context *context,
    const char *id, bool *enabled, uint32_t *rgb) {
    struct nk_rect cell;
    if (nk_widget(&cell, context) == NK_WIDGET_INVALID) return false;
    bool changed = toggle_at(context, id, enabled, cell, true);
    if (*enabled) draw_color_chip(context, cell, *rgb);
    return changed;
}

static struct nk_color hsv_to_rgb(float h, float s, float v) {
    float i = h * 6.0f;
    int sector = (int)i;
    float f = i - (float)sector;
    float e = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);
    float r, g, b;
    switch (sector % 6) {
    case 0: r = v; g = t; b = e; break;
    case 1: r = q; g = v; b = e; break;
    case 2: r = e; g = v; b = t; break;
    case 3: r = e; g = q; b = v; break;
    case 4: r = t; g = e; b = v; break;
    default: r = v; g = e; b = q; break;
    }
    return nk_rgb((nk_byte)(r * 255.0f + 0.5f),
        (nk_byte)(g * 255.0f + 0.5f), (nk_byte)(b * 255.0f + 0.5f));
}

static void rgb_to_hsv(struct nk_color color, float *h, float *s, float *v) {
    float r = color.r / 255.0f, g = color.g / 255.0f, b = color.b / 255.0f;
    float maximum = NK_MAX(r, NK_MAX(g, b));
    float minimum = NK_MIN(r, NK_MIN(g, b));
    float delta = maximum - minimum;
    *v = maximum;
    *s = maximum > 0.0f ? delta / maximum : 0.0f;
    if (delta <= 0.0f) *h = 0.0f;
    else if (maximum == r) *h = (g - b) / delta / 6.0f + (g < b ? 1.0f : 0.0f);
    else if (maximum == g) *h = (b - r) / delta / 6.0f + 1.0f / 3.0f;
    else *h = (r - g) / delta / 6.0f + 2.0f / 3.0f;
}

/* Palette for the OBS solid background: a saturation/value square plus a
   hue slider. Hold the left button to adjust continuously. */
bool bongo_cat_pref_color_picker(struct nk_context *context, const char *id,
    uint32_t *rgb) {
    (void)id;
    nk_layout_row_dynamic(context, 172.0f, 1);
    struct nk_rect cell;
    if (nk_widget(&cell, context) == NK_WIDGET_INVALID) return false;
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    nk_fill_rect(canvas, cell, 12, p.field);
    const float pad = 10.0f, bar_width = 22.0f, gap = 10.0f;
    struct nk_rect square = nk_rect(cell.x + pad, cell.y + pad,
        cell.w - 2.0f * pad - bar_width - gap, cell.h - 2.0f * pad);
    struct nk_rect bar = nk_rect(square.x + square.w + gap, cell.y + pad,
        bar_width, cell.h - 2.0f * pad);
    float hue, saturation, value;
    rgb_to_hsv(nk_rgb((*rgb >> 16) & 255, (*rgb >> 8) & 255, *rgb & 255),
        &hue, &saturation, &value);
    struct nk_color hue_color = hsv_to_rgb(hue, 1.0f, 1.0f);
    nk_fill_rect_multi_color(canvas, square, nk_rgb(255, 255, 255),
        hue_color, nk_rgb(0, 0, 0), nk_rgb(0, 0, 0));
    nk_stroke_rect(canvas, square, 4, 1.0f, p.border);
    /* The hue gradient as six bilinear strips: red through magenta back to
       red, so the slider covers the full circle. */
    const float segment_height = bar.h / 6.0f;
    for (int i = 0; i < 6; ++i) {
        struct nk_color start = hsv_to_rgb(i / 6.0f, 1.0f, 1.0f);
        struct nk_color end = hsv_to_rgb((i + 1) / 6.0f, 1.0f, 1.0f);
        nk_fill_rect_multi_color(canvas,
            nk_rect(bar.x, bar.y + segment_height * i, bar.w,
                segment_height + 1.0f),
            start, end, start, end);
    }
    nk_stroke_rect(canvas, bar, 4, 1.0f, p.border);
    struct nk_input *input = &context->input;
    bool grab_square =
        nk_input_is_mouse_click_in_rect(input, NK_BUTTON_LEFT, square);
    bool grab_bar = nk_input_is_mouse_click_in_rect(input, NK_BUTTON_LEFT, bar);
    bool changed = false;
    if (grab_square) {
        saturation = NK_CLAMP(0.0f,
            (input->mouse.pos.x - square.x) / square.w, 1.0f);
        value = NK_CLAMP(0.0f,
            1.0f - (input->mouse.pos.y - square.y) / square.h, 1.0f);
        changed = true;
    } else if (grab_bar) {
        hue = NK_CLAMP(0.0f,
            (input->mouse.pos.y - bar.y) / bar.h, 0.9999f);
        changed = true;
    }
    if (changed) {
        struct nk_color color = hsv_to_rgb(hue, saturation, value);
        *rgb = ((uint32_t)color.r << 16) | ((uint32_t)color.g << 8) |
            color.b;
    }
    /* Saturation/value crosshair marker. */
    float marker_x = square.x + saturation * square.w;
    float marker_y = square.y + (1.0f - value) * square.h;
    struct nk_rect marker = nk_rect(marker_x - 6.0f, marker_y - 6.0f,
        12.0f, 12.0f);
    nk_fill_circle(canvas, marker, nk_rgb(255, 255, 255));
    nk_stroke_circle(canvas, marker, 1.5f, nk_rgb(0, 0, 0));
    /* Hue handle. */
    float handle_y = bar.y + hue * bar.h;
    struct nk_rect handle = nk_rect(bar.x + bar.w * .5f - 7.0f,
        handle_y - 7.0f, 14.0f, 14.0f);
    nk_fill_circle(canvas, handle, nk_rgb(255, 255, 255));
    nk_stroke_circle(canvas, handle, 1.5f, nk_rgb(0, 0, 0));
    if (nk_input_is_mouse_hovering_rect(input, square) ||
        nk_input_is_mouse_hovering_rect(input, bar))
        bongo_cat_ui_cursor_hover_rect(context, cell,
            BONGO_CAT_UI_CURSOR_POINTER);
    return changed;
}
