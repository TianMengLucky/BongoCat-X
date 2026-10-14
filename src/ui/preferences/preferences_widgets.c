#include "preferences_widgets.h"
#include "preferences_widgets_internal.h"
#include "preferences_controls.h"
#include "preferences_theme.h"
#include "ui_backend.h"
#include "ui_catime.h"
#include "ui_paint.h"
#include "ui_tooltip.h"
#include "../rendering/ui_icons.h"
bool bongo_cat_pref_card_begin(struct nk_context *context, const char *id,
    struct nk_style_window *saved) {
    *saved = context->style.window;
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    context->style.window.fixed_background = nk_style_item_color(p.surface);
    context->style.window.background = p.surface;
    context->style.window.group_border = 1;
    context->style.window.group_border_color = p.border_subtle;
    context->style.window.rounding = 14;
    context->style.window.group_padding = nk_vec2(18, 14);
    context->style.window.spacing = nk_vec2(10, 8);
    if (nk_group_begin(context, id, NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) return true;
    context->style.window = *saved;
    return false;
}
void bongo_cat_pref_card_end(struct nk_context *context, const struct nk_style_window *saved) {
    nk_group_end(context);
    context->style.window = *saved;
}
void bongo_cat_pref_intro(struct nk_context *context, const char *text) {
    float width = NK_MAX(1.0f, nk_window_get_content_region(context).w - 32.0f);
    int lines = bongo_cat_pref_detail_text(context, text, nk_rect(0, 0, width, 0), false);
    nk_layout_row_dynamic(context, NK_MAX(52.0f, 28.0f + (float)lines * 19.0f), 1);
    struct nk_rect bounds;
    if (nk_widget(&bounds, context) == NK_WIDGET_INVALID) return;
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    nk_fill_rect(canvas, bounds, 12, p.hover);
    nk_stroke_rect(canvas, bounds, 12, 1, p.border);
    bounds.x += 16; bounds.y += 14; bounds.w -= 32; bounds.h -= 28;
    (void)bongo_cat_pref_detail_text(context, text, bounds, true);
}
static struct nk_context *section_context;
static bool section_first;
void bongo_cat_pref_section_rows_begin(struct nk_context *context) {
    section_context = context; section_first = true;
}
static const float description_line_height = 19.0f;
int bongo_cat_pref_detail_lines(const struct nk_context *context,
    const char *text) {
    float width = NK_MAX(1.0f,
        nk_window_get_content_region(context).w - 26.0f - 33.0f);
    return bongo_cat_pref_detail_text(context,
        text, nk_rect(0, 0, width, 0), false);
}
static void restore_style(struct nk_context *context, const FormStyle *saved) {
    context->style.window.fixed_background = saved->background;
    context->style.window.background = saved->window_color;
    context->style.window.group_border_color = saved->border_color;
    context->style.window.group_padding = saved->padding;
    context->style.window.spacing = saved->spacing;
    context->style.window.group_border = saved->border;
}
bool bongo_cat_pref_form_begin(struct nk_context *context, const char *id,
    int lines, FormStyle *saved) {
    saved->background = context->style.window.fixed_background;
    saved->window_color = context->style.window.background;
    saved->border_color = context->style.window.group_border_color;
    saved->padding = context->style.window.group_padding;
    saved->spacing = context->style.window.spacing;
    saved->border = context->style.window.group_border;
    struct nk_color clear = nk_rgba(0, 0, 0, 0);
    context->style.window.fixed_background = nk_style_item_color(clear);
    context->style.window.background = clear;
    context->style.window.group_border_color = clear;
    context->style.window.group_padding = nk_vec2(13, 11);
    context->style.window.spacing = nk_vec2(8, 3);
    context->style.window.group_border = 0;
    float height = lines ? 66.0f + description_line_height * lines : 68.0f;
    if (section_context == context && section_first && !lines) height = 68.0f;
    if (section_context == context) section_first = false;
    nk_layout_row_dynamic(context, height, 1);
    if (!nk_group_begin(context, id, NK_WINDOW_NO_SCROLLBAR)) {
        bongo_cat_pref_row_icon_clear(context);
        restore_style(context, saved);
        return false;
    }
    return true;
}
void bongo_cat_pref_form_end(struct nk_context *context,
    const FormStyle *saved) {
    bongo_cat_pref_row_icon_clear(context);
    nk_group_end(context); restore_style(context, saved); }
#define form_begin bongo_cat_pref_form_begin
#define form_end bongo_cat_pref_form_end
static void form_title(struct nk_context *context, const char *title) {
    bongo_cat_pref_form_title_sized(context, title, 220.0f); }
void bongo_cat_pref_description(struct nk_context *context, const char *text,
    int lines) {
    if (!lines) return;
    nk_layout_row_dynamic(context, description_line_height * lines, 1);
    struct nk_rect bounds;
    if (nk_widget(&bounds, context) == NK_WIDGET_INVALID) return;
    bounds.x += 33.0f;
    bounds.w = NK_MAX(1.0f, bounds.w - 33.0f);
    bongo_cat_pref_detail_text(context, text, bounds, true);
}
static bool secondary_button(struct nk_context *context, const char *label) {
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    struct nk_style_button style = context->style.button;
    style.normal = nk_style_item_color(p.field);
    style.hover = nk_style_item_color(p.selection);
    style.active = nk_style_item_color(p.selection);
    style.border_color = p.border_subtle;
    style.border = 1;
    style.rounding = 10;
    style.text_normal = p.text;
    style.text_hover = p.accent;
    style.text_active = p.accent;
    struct nk_rect bounds = nk_widget_bounds(context);
    if (nk_input_is_mouse_hovering_rect(&context->input, bounds))
        bongo_cat_ui_cursor_hover_rect(context, bounds, BONGO_CAT_UI_CURSOR_POINTER);
    return nk_button_label_styled(context, &style, label) != 0;
}
void bongo_cat_pref_section(struct nk_context *context, const char *title) {
    section_context = context; section_first = true;
    struct nk_rect bounds;
    nk_layout_row_dynamic(context, 22, 1);
    if (nk_widget(&bounds, context) == NK_WIDGET_INVALID) return;
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    nk_fill_rect(canvas, nk_rect(bounds.x, bounds.y + 2, 4, 18), 2, p.pink);
    const struct nk_user_font *font = bongo_cat_ui_label_font(context);
    struct nk_rect text = nk_rect(bounds.x + 14,
        bounds.y + (bounds.h - font->height) * .5f, bounds.w - 14, font->height);
    nk_draw_text(canvas, text, title, nk_strlen(title), font,
        nk_rgba(0, 0, 0, 0), p.text);
}
void bongo_cat_pref_section_icon(struct nk_context *context,
    const char *title, BongoCatPrefIcon icon) {
    section_context = context; section_first = true;
    const struct nk_user_font *font = bongo_cat_ui_label_font(context);
    float width = nk_window_get_content_region(context).w - 42;
    float measured = font->width(font->userdata, font->height,
        title, nk_strlen(title));
    if (width > 0 && measured > width) {
        /* Leave room for word wrapping in longer translations. */
        float height = (float)((int)(measured / width) + 2) *
            NK_MAX(22.0f, font->height + 2.0f);
        struct nk_vec2 spacing = context->style.window.spacing;
        struct nk_vec2 padding = context->style.text.padding;
        context->style.window.spacing.x = 0;
        context->style.text.padding = nk_vec2(0, 0);
        nk_style_push_font(context, font);
        nk_layout_row_begin(context, NK_STATIC, height, 2);
        nk_layout_row_push(context, 42);
        struct nk_rect bounds;
        if (nk_widget(&bounds, context) != NK_WIDGET_INVALID) {
            BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
            struct nk_command_buffer *canvas = nk_window_get_canvas(context);
            nk_fill_rect(canvas, nk_rect(bounds.x, bounds.y + 2, 4, 18), 2, p.pink);
            bongo_cat_pref_icon_draw(canvas,
                nk_rect(bounds.x + 14, bounds.y + 2, 18, 18), icon, p.accent);
        }
        nk_layout_row_push(context, width);
        nk_text_wrap_colored(context, title, nk_strlen(title),
            bongo_cat_ui_palette(bongo_cat_ui_dark(context)).accent);
        nk_layout_row_end(context);
        nk_style_pop_font(context);
        context->style.window.spacing = spacing;
        context->style.text.padding = padding;
        return;
    }
    struct nk_rect bounds;
    nk_layout_row_dynamic(context, 22, 1);
    if (nk_widget(&bounds, context) == NK_WIDGET_INVALID) return;
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    nk_fill_rect(canvas, nk_rect(bounds.x, bounds.y + 2, 4, 18), 2, p.pink);
    struct nk_rect icon_bounds = nk_rect(bounds.x + 14,
        bounds.y + 2, 18, 18);
    bongo_cat_pref_icon_draw(canvas, icon_bounds, icon, p.accent);
    struct nk_rect text = nk_rect(bounds.x + 42,
        bounds.y + (bounds.h - font->height) * .5f,
        bounds.w - 42, font->height);
    nk_draw_text(canvas, text, title, nk_strlen(title), font,
        nk_rgba(0, 0, 0, 0), p.accent);
}
bool bongo_cat_pref_toggle(struct nk_context *context, const char *id,
    const char *title, const char *detail, bool *value) {
    int lines = bongo_cat_pref_detail_lines(context, detail); FormStyle saved;
    if (!form_begin(context, id, lines, &saved)) return false;
    bongo_cat_pref_form_title_sized(context, title, 80.0f);
    bool changed = bongo_cat_pref_control_toggle(context, id, value);
    nk_layout_row_end(context); bongo_cat_pref_description(context, detail, lines);
    form_end(context, &saved); return changed;
}
bool bongo_cat_pref_toggle_help(struct nk_context *context, const char *id,
    const char *title, const char *description, const char *help, bool *value) {
    FormStyle saved;
    int lines = bongo_cat_pref_detail_lines(context, description);
    if (!form_begin(context, id, lines, &saved)) return false;
    bongo_cat_pref_form_title_sized(context, title, 80.0f);
    bool changed = bongo_cat_pref_control_toggle(context, id, value);
    nk_layout_row_end(context);
    bongo_cat_ui_question_tooltip(context, description, help);
    form_end(context, &saved);
    return changed;
}
bool bongo_cat_pref_obs_background(struct nk_context *context, const char *id,
    const char *title, const char *question, const char *reply, bool *enabled,
    uint32_t *rgb) {
    int lines = bongo_cat_pref_detail_lines(context, question); FormStyle saved;
    if (!form_begin(context, id, lines, &saved)) return false;
    bongo_cat_pref_form_title_sized(context, title, 230.0f);
    bool changed = bongo_cat_pref_control_obs_background(context, id, enabled, rgb);
    nk_layout_row_end(context);
    bongo_cat_ui_question_tooltip(context, question, reply);
    form_end(context, &saved); return changed;
}
bool bongo_cat_pref_float(struct nk_context *context, const char *id,
    const char *title, const char *detail, float minimum, float *value,
    float maximum, float step, float default_value) {
    int lines = bongo_cat_pref_detail_lines(context, detail); FormStyle saved;
    if (!form_begin(context, id, lines, &saved)) return false;
    form_title(context, title);
    bool changed = bongo_cat_pref_control_float(context, id,
        minimum, value, maximum, step, default_value);
    nk_layout_row_end(context); bongo_cat_pref_description(context, detail, lines);
    form_end(context, &saved); return changed;
}
bool bongo_cat_pref_float_action(struct nk_context *context, const char *id,
    const char *title, const char *detail, float minimum, float *value,
    float maximum, float step, float default_value, const char *button) {
    float detail_width = NK_MAX(1.0f,
        nk_window_get_content_region(context).w - 26.0f - 33.0f);
    int lines = bongo_cat_pref_detail_text(context, detail,
        nk_rect(0, 0, detail_width, 0), false);
    FormStyle saved;
    if (!form_begin(context, id, lines, &saved)) return false;
    const struct nk_user_font *font = context->style.font;
    float button_width = NK_MAX(88.0f, font->width(font->userdata,
        font->height, button, nk_strlen(button)) + 24.0f);
    float available = nk_window_get_content_region(context).w;
    nk_layout_row_begin(context, NK_STATIC, 36, 3);
    nk_layout_row_push(context, NK_MAX(1.0f, available - 124.0f -
        button_width - 16.0f));
    bongo_cat_pref_form_label(context, title);
    nk_layout_row_push(context, 124.0f);
    bongo_cat_pref_control_float(context, id,
        minimum, value, maximum, step, default_value);
    nk_layout_row_push(context, button_width);
    bool clicked = bongo_cat_pref_capsule_button(context, id, button);
    nk_layout_row_end(context);
    if (lines) {
        nk_layout_row_dynamic(context, 19.0f * lines, 1);
        struct nk_rect bounds;
        if (nk_widget(&bounds, context) != NK_WIDGET_INVALID) {
            bounds.x += 33.0f;
            bounds.w = detail_width;
            bongo_cat_pref_detail_text(context, detail, bounds, true);
        }
    }
    form_end(context, &saved); return clicked;
}

/* float_action with an extra icon button (e.g. restore the default size);
   the icon shows a question tooltip on hover. */
static bool default_size_button(struct nk_context *context,
    const char *id, const char *hint) {
    (void)id;
    struct nk_rect bounds;
    if (nk_widget(&bounds, context) == NK_WIDGET_INVALID) return false;
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    bool hover = nk_input_is_mouse_hovering_rect(&context->input, bounds);
    nk_fill_rect(canvas, bounds, 10, hover ? p.hover : p.field);
    nk_stroke_rect(canvas, bounds, 10, 1, hover ? p.accent : p.border_subtle);
    struct nk_rect icon = nk_rect(bounds.x + (bounds.w - 16.0f) * .5f,
        bounds.y + (bounds.h - 16.0f) * .5f, 16.0f, 16.0f);
    bongo_cat_ui_draw_icon(canvas, BONGO_CAT_UI_ICON_SYNC, icon,
        hover ? p.accent : p.muted);
    if (hover) {
        bongo_cat_ui_cursor_hover_rect(context, bounds,
            BONGO_CAT_UI_CURSOR_POINTER);
        bongo_cat_ui_question_tooltip(context, hint, "");
    }
    return hover && nk_input_is_mouse_click_in_rect(&context->input,
        NK_BUTTON_LEFT, bounds);
}

bool bongo_cat_pref_float_action_default(struct nk_context *context,
    const char *id, const char *title, const char *detail, float minimum,
    float *value, float maximum, float step, float default_value,
    const char *button, const char *default_hint, bool *default_clicked) {
    float detail_width = NK_MAX(1.0f,
        nk_window_get_content_region(context).w - 26.0f - 33.0f);
    int lines = bongo_cat_pref_detail_text(context, detail,
        nk_rect(0, 0, detail_width, 0), false);
    FormStyle saved;
    if (!form_begin(context, id, lines, &saved)) return false;
    const struct nk_user_font *font = context->style.font;
    float button_width = NK_MAX(88.0f, font->width(font->userdata,
        font->height, button, nk_strlen(button)) + 24.0f);
    float available = nk_window_get_content_region(context).w;
    nk_layout_row_begin(context, NK_STATIC, 36, 4);
    nk_layout_row_push(context, NK_MAX(1.0f, available - 124.0f -
        button_width - 36.0f - 24.0f));
    bongo_cat_pref_form_label(context, title);
    nk_layout_row_push(context, 124.0f);
    bongo_cat_pref_control_float(context, id,
        minimum, value, maximum, step, default_value);
    nk_layout_row_push(context, button_width);
    bool clicked = bongo_cat_pref_capsule_button(context, id, button);
    nk_layout_row_push(context, 36.0f);
    if (default_clicked) *default_clicked =
        default_size_button(context, id, default_hint);
    nk_layout_row_end(context);
    if (lines) {
        nk_layout_row_dynamic(context, 19.0f * lines, 1);
        struct nk_rect bounds;
        if (nk_widget(&bounds, context) != NK_WIDGET_INVALID) {
            bounds.x += 33.0f;
            bounds.w = detail_width;
            bongo_cat_pref_detail_text(context, detail, bounds, true);
        }
    }
    form_end(context, &saved); return clicked;
}
bool bongo_cat_pref_int(struct nk_context *context, const char *id,
    const char *title, const char *detail, int minimum, int *value,
    int maximum, int step, int default_value) {
    int lines = bongo_cat_pref_detail_lines(context, detail); FormStyle saved;
    if (!form_begin(context, id, lines, &saved)) return false;
    form_title(context, title);
    bool changed = bongo_cat_pref_control_int(context, id,
        minimum, value, maximum, step, default_value);
    nk_layout_row_end(context); bongo_cat_pref_description(context, detail, lines);
    form_end(context, &saved); return changed;
}
bool bongo_cat_pref_slider(struct nk_context *context, const char *id,
    const char *title, const char *detail, float minimum, float *value,
    float maximum, float step, float default_value) {
    return bongo_cat_pref_slider_suffix(context, id, title, detail, minimum,
        value, maximum, step, default_value, "%");
}

bool bongo_cat_pref_slider_suffix(struct nk_context *context, const char *id,
    const char *title, const char *detail, float minimum, float *value,
    float maximum, float step, float default_value, const char *suffix) {
    int lines = bongo_cat_pref_detail_lines(context, detail); FormStyle saved;
    if (!form_begin(context, id, lines, &saved)) return false;
    form_title(context, title);
    bool changed = bongo_cat_pref_control_slider(context, id,
        minimum, value, maximum, step, default_value, suffix, 0.0f);
    nk_layout_row_end(context); bongo_cat_pref_description(context, detail, lines);
    form_end(context, &saved); return changed;
}
int bongo_cat_pref_combo(struct nk_context *context, const char *id,
    const char *title, const char *detail, const char *const *items,
    int count, int selected) {
    int lines = bongo_cat_pref_detail_lines(context, detail); FormStyle saved;
    if (!form_begin(context, id, lines, &saved)) return selected;
    bongo_cat_pref_form_title_sized(context, title, 176.0f);
    selected = bongo_cat_pref_control_combo(context, id,
        items, count, selected);
    nk_layout_row_end(context); bongo_cat_pref_description(context, detail, lines);
    form_end(context, &saved); return selected;
}
bool bongo_cat_pref_button(struct nk_context *context, const char *id,
    const char *title, const char *detail, const char *button) {
    int lines = bongo_cat_pref_detail_lines(context, detail); FormStyle saved;
    if (!form_begin(context, id, lines, &saved)) return false;
    form_title(context, title); bool result = secondary_button(context, button);
    nk_layout_row_end(context); bongo_cat_pref_description(context, detail, lines);
    form_end(context, &saved); return result;
}
static bool icon_action_button(struct nk_context *context, int icon) {
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    struct nk_rect bounds;
    /* Consume the widget slot: a bounds peek leaves it to the next widget,
       which then paints over this button entirely. */
    enum nk_widget_layout_states state = nk_widget(&bounds, context);
    if (state != NK_WIDGET_VALID && state != NK_WIDGET_ROM) return false;
    bool hovered = nk_input_is_mouse_hovering_rect(&context->input, bounds);
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    if (hovered)
        bongo_cat_ui_cursor_hover_rect(context, bounds,
            BONGO_CAT_UI_CURSOR_POINTER);
    nk_fill_rect(canvas, bounds, 10, hovered ? p.selection : p.field);
    nk_stroke_rect(canvas, bounds, 10, 1, p.border_subtle);
    float icon_size = NK_MAX(0.0f, NK_MIN(bounds.w, bounds.h) - 14.0f);
    struct nk_rect icon_bounds = nk_rect(
        bounds.x + (bounds.w - icon_size) * .5f,
        bounds.y + (bounds.h - icon_size) * .5f, icon_size, icon_size);
    bongo_cat_ui_draw_icon(canvas, icon, icon_bounds,
        hovered ? p.accent : p.text);
    return hovered && nk_input_is_mouse_click_in_rect(&context->input,
        NK_BUTTON_LEFT, bounds);
}
int bongo_cat_pref_button_with_icon(struct nk_context *context,
    const char *id, const char *title, const char *detail, int icon,
    const char *button) {
    int lines = bongo_cat_pref_detail_lines(context, detail); FormStyle saved;
    if (!form_begin(context, id, lines, &saved)) return 0;
    float available = nk_window_get_content_region(context).w;
    float left = NK_MAX(220.0f, available - 250.0f);
    nk_layout_row_begin(context, NK_STATIC, 36, 3);
    nk_layout_row_push(context, left);
    bongo_cat_pref_form_label(context, title);
    nk_layout_row_push(context, 36);
    bool detect = icon_action_button(context, icon);
    nk_layout_row_push(context, NK_MAX(140.0f, available - left - 44.0f));
    bool main = secondary_button(context, button);
    nk_layout_row_end(context);
    bongo_cat_pref_description(context, detail, lines);
    form_end(context, &saved);
    return main ? 1 : detect ? 2 : 0;
}
void bongo_cat_pref_status(struct nk_context *context, const char *id,
    const char *title, const char *detail) {
    int lines = bongo_cat_pref_detail_lines(context, detail); FormStyle saved;
    if (!form_begin(context, id, lines, &saved)) return;
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    nk_layout_row_dynamic(context, 44, 1);
    struct nk_rect row; nk_widget(&row, context);
    nk_fill_circle(nk_window_get_canvas(context),
        nk_rect(row.x, row.y + (row.h - 10) * .5f, 10, 10), p.accent);
    const struct nk_user_font *font = bongo_cat_ui_label_font(context);
    struct nk_rect text = nk_rect(row.x + 20,
        row.y + (row.h - font->height) * .5f, row.w - 20, font->height);
    nk_draw_text(nk_window_get_canvas(context), text, title, nk_strlen(title),
        font, nk_rgba(0, 0, 0, 0), p.text);
    bongo_cat_pref_description(context, detail, lines);
    form_end(context, &saved);
}
