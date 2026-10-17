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

static bool color_picker_update(struct nk_context *context,
    BongoCatPrefColorState *state, struct nk_rect square, struct nk_rect bar,
    uint32_t *rgb, bool available) {
    if (!state->initialized || state->rgb != *rgb) {
        nk_color_hsv_f(&state->hue, &state->saturation, &state->value,
            nk_rgb((*rgb >> 16) & 255, (*rgb >> 8) & 255, *rgb & 255));
        state->initialized = true;
        state->rgb = *rgb;
    }
    struct nk_input *input = &context->input;
    if (!available || !nk_input_has_mouse_click_down_in_rect(input,
        NK_BUTTON_LEFT, context->current->layout->clip, nk_true)) return false;
    if (nk_input_has_mouse_click_down_in_rect(input, NK_BUTTON_LEFT,
        square, nk_true)) {
        state->saturation = NK_CLAMP(0.0f,
            (input->mouse.pos.x - square.x) / square.w, 1.0f);
        state->value = NK_CLAMP(0.0f,
            1.0f - (input->mouse.pos.y - square.y) / square.h, 1.0f);
    } else if (nk_input_has_mouse_click_down_in_rect(input, NK_BUTTON_LEFT,
        bar, nk_true)) {
        state->hue = NK_CLAMP(0.0f,
            (input->mouse.pos.y - bar.y) / bar.h, 1.0f);
    } else return false;
    struct nk_color color = nk_hsv_f(state->hue, state->saturation, state->value);
    state->rgb = ((uint32_t)color.r << 16) | ((uint32_t)color.g << 8) | color.b;
    bool changed = *rgb != state->rgb;
    *rgb = state->rgb;
    return changed;
}

/* Shared theme/OBS picker; apply input before painting the gradient and markers. */
static bool color_picker_panel(struct nk_context *context,
    BongoCatPrefColorState *state, uint32_t *rgb) {
    nk_layout_row_dynamic(context, 172.0f, 1);
    struct nk_rect cell;
    enum nk_widget_layout_states widget = nk_widget(&cell, context);
    if (widget == NK_WIDGET_INVALID) return false;
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    nk_fill_rect(canvas, cell, 12, p.field);
    const float pad = 10.0f, bar_width = 22.0f, gap = 10.0f;
    struct nk_rect square = nk_rect(cell.x + pad, cell.y + pad,
        cell.w - 2.0f * pad - bar_width - gap, cell.h - 2.0f * pad);
    struct nk_rect bar = nk_rect(square.x + square.w + gap, cell.y + pad,
        bar_width, cell.h - 2.0f * pad);
    if (square.w <= 0.0f || square.h <= 0.0f) return false;
    bool changed = color_picker_update(context, state, square, bar, rgb,
        widget != NK_WIDGET_DISABLED &&
        !(context->current->layout->flags & NK_WINDOW_ROM));
    float hue = state->hue, saturation = state->saturation, value = state->value;
    struct nk_color hue_color = nk_hsv_f(hue, 1.0f, 1.0f);
    nk_fill_rect_multi_color(canvas, square, nk_rgb(255, 255, 255),
        hue_color, nk_rgb(0, 0, 0), nk_rgb(0, 0, 0));
    nk_stroke_rect(canvas, square, 4, 1.0f, p.border);
    /* The hue gradient as six bilinear strips: red through magenta back to
       red, so the slider covers the full circle. */
    const float segment_height = bar.h / 6.0f;
    for (int i = 0; i < 6; ++i) {
        struct nk_color start = nk_hsv_f(i / 6.0f, 1.0f, 1.0f);
        struct nk_color end = nk_hsv_f((i + 1) / 6.0f, 1.0f, 1.0f);
        nk_fill_rect_multi_color(canvas,
            nk_rect(bar.x, bar.y + segment_height * i, bar.w,
                segment_height + 1.0f),
            start, start, end, end);
    }
    nk_stroke_rect(canvas, bar, 4, 1.0f, p.border);
    struct nk_input *input = &context->input;
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
/* All color options use the same swatch button and popup. */
bool bongo_cat_pref_color_picker(struct nk_context *context,
    BongoCatPrefColorState *state, uint32_t *rgb) {
    struct nk_rect bounds = nk_widget_bounds(context);
    if (nk_input_is_mouse_hovering_rect(&context->input, bounds))
        bongo_cat_ui_cursor_hover_rect(context, bounds,
            BONGO_CAT_UI_CURSOR_POINTER);
    if (!nk_combo_begin_color(context,
        nk_rgb((*rgb >> 16) & 255, (*rgb >> 8) & 255, *rgb & 255),
        nk_vec2(280.0f, 196.0f))) return false;
    bool changed = color_picker_panel(context, state, rgb);
    nk_combo_end(context);
    return changed;
}
