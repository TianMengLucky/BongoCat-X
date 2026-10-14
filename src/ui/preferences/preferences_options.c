#include "preferences_state.h"
#include "preferences_options.h"
#include "ui_paint.h"
#include "preferences_icons_internal.h"

bool bongo_cat_preferences_options_begin(BongoCatPreferences *value,
    struct nk_context *context, enum BongoCatOptionsSection section,
    const char *title, BongoCatPrefIcon icon) {
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    struct nk_style_button style = context->style.button;
    bool open = value->options_sections[section] == NK_MAXIMIZED;
    style.normal = nk_style_item_color(open ? p.selection : p.surface);
    style.hover = nk_style_item_color(p.hover);
    style.active = nk_style_item_color(p.selection);
    style.border_color = open ? p.border : p.border_subtle;
    style.border = 1;
    style.rounding = 12;
    nk_layout_row_dynamic(context, 48, 1);
    struct nk_rect bounds = nk_widget_bounds(context);
    if (nk_button_label_styled(context, &style, "")) {
        open = !open;
        value->options_sections[section] = open ? NK_MAXIMIZED : NK_MINIMIZED;
        value->render_dirty = true;
    }
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    struct nk_rect glyph = nk_rect(bounds.x + 16, bounds.y + 14, 20, 20);
    (void)bongo_cat_pref_section_icon_draw(canvas, glyph, icon, p.accent);
    const struct nk_user_font *font = bongo_cat_ui_label_font(context);
    struct nk_rect text = nk_rect(bounds.x + 48,
        bounds.y + (bounds.h - font->height) * .5f, bounds.w - 84, font->height);
    nk_draw_text(canvas, text, title, nk_strlen(title), font,
        nk_rgba(0, 0, 0, 0), p.text);
    float x = bounds.x + bounds.w - 24, y = bounds.y + bounds.h * .5f;
    if (open) {
        nk_stroke_line(canvas, x - 4, y - 2, x, y + 2, 1.5f, p.muted);
        nk_stroke_line(canvas, x, y + 2, x + 4, y - 2, 1.5f, p.muted);
    } else {
        nk_stroke_line(canvas, x - 2, y - 4, x + 2, y, 1.5f, p.muted);
        nk_stroke_line(canvas, x + 2, y, x - 2, y + 4, 1.5f, p.muted);
    }
    if (nk_input_is_mouse_hovering_rect(&context->input, bounds))
        bongo_cat_ui_cursor_hover_rect(context, bounds, BONGO_CAT_UI_CURSOR_POINTER);
    if (open) bongo_cat_pref_section_rows_begin(context);
    return open;
}

void bongo_cat_preferences_options_end(BongoCatPreferences *value, struct nk_context *context) {
    (void)value;
    nk_layout_row_dynamic(context, 6, 1);
    nk_spacing(context, 1);
}
