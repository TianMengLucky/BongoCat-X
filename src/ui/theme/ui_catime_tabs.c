#include "ui_catime.h"
#include "ui_animation.h"
#include "ui_backend.h"
#include "ui_paint.h"

#include <stdio.h>

static void tab_content(struct nk_command_buffer *canvas,
    struct nk_context *context, struct nk_rect tile, const char *label,
    int icon, struct nk_color color, BongoCatUIIconDraw draw_icon,
    void *icon_userdata) {
    const struct nk_user_font *font = bongo_cat_ui_body_font(context);
    bool show_label = tile.w >= 120.0f;
    float icon_size = 20.0f;
    struct nk_rect icon_bounds = nk_rect(show_label ? tile.x + 14.0f :
        tile.x + (tile.w - icon_size) * .5f,
        tile.y + (tile.h - icon_size) * .5f, icon_size, icon_size);
    if (draw_icon) draw_icon(icon_userdata, canvas, icon, icon_bounds, color);
    else bongo_cat_ui_fallback_icon(canvas, icon, icon_bounds, color);
    if (show_label) {
        struct nk_rect text = nk_rect(tile.x + 46.0f,
            tile.y + (tile.h - font->height) * .5f,
            tile.w - 58.0f, font->height);
        nk_draw_text(canvas, text, label, nk_strlen(label), font,
            nk_rgba(0, 0, 0, 0), color);
        show_label = font->width(font->userdata, font->height, label,
            nk_strlen(label)) <= text.w;
    }
    if (!show_label && nk_input_is_mouse_hovering_rect(&context->input, tile))
        nk_tooltip(context, label);
}

void bongo_cat_ui_tabs(struct nk_context *context, const char *const *labels,
    const int *icons, int count, int *active, bool interactive, bool dark,
    float available_height, bool about_badge,
    BongoCatUIIconDraw draw_icon, void *icon_userdata) {
    BongoCatUIPalette p = bongo_cat_ui_palette(dark);
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    float header_height = nk_window_get_content_region(context).w < 100.0f ?
        118.0f : 128.0f;
    float room = NK_MAX(1.0f, available_height - header_height);
    if (count <= 0) return;
    /* Distribute the menu over the sidebar even when optional pages are off.
       Keep the tiles compact within their slots instead of stretching them. */
    float row_height = NK_MAX(40.0f, room / count - 8.0f);
    float saved_spacing = context->style.window.spacing.y;
    context->style.window.spacing.y = 8.0f;
    for (int i = 0; i < count; ++i) {
        struct nk_rect bounds;
        nk_layout_row_dynamic(context, row_height, 1);
        if (nk_widget(&bounds, context) == NK_WIDGET_INVALID) continue;
        float tile_height = NK_MIN(46.0f, bounds.h);
        struct nk_rect hit = nk_rect(bounds.x + 10,
            bounds.y + (bounds.h - tile_height) * .5f + 1,
            bounds.w - 20, tile_height - 1);
        bool hover = interactive &&
            nk_input_is_mouse_hovering_rect(&context->input, hit);
        bool selected = *active == i;
        char selection_id[32], hover_id[32];
        snprintf(selection_id, sizeof(selection_id), "sidebar-active-%d", i);
        snprintf(hover_id, sizeof(hover_id), "sidebar-hover-%d", i);
        float weight = bongo_cat_ui_animate_eased(context, selection_id,
            selected ? 1.0f : 0.0f, 200.0f, BONGO_CAT_UI_EASE_SWIFT);
        float hover_weight = bongo_cat_ui_animate_eased(context, hover_id,
            hover ? 1.0f : 0.0f, 200.0f, BONGO_CAT_UI_EASE_SWIFT);
        struct nk_rect tile = hit;
        if (hover_weight > 0.0f) nk_fill_rect(canvas, tile, 10,
            bongo_cat_ui_color_alpha(p.hover, hover_weight));
        if (selected && p.effects) bongo_cat_ui_paint_shadow(context,
            tile, 8, 0, 5, 14, 0,
            nk_rgba(p.accent.r, p.accent.g, p.accent.b, 45));
        if (weight > 0.0f) {
            nk_fill_rect(canvas, tile, 10,
                nk_rgba(p.selection.r, p.selection.g, p.selection.b, (nk_byte)(255 * weight)));
            nk_fill_rect(canvas, nk_rect(tile.x + 5, tile.y + 12, 3, tile.h - 24), 1.5f,
                nk_rgba(p.accent.r, p.accent.g, p.accent.b, (nk_byte)(255 * weight)));
        }
        struct nk_color color = selected ? p.on_selection : hover ? p.text : p.muted;
        tab_content(canvas, context, tile, labels[i], icons ? icons[i] : i, color,
            draw_icon, icon_userdata);
        if (about_badge && i == count - 1) {
            nk_fill_circle(canvas, nk_rect(tile.x + tile.w - 11.0f,
                tile.y + 7.0f, 7.0f, 7.0f), p.danger);
        }
        if (hover) bongo_cat_ui_cursor_hover_rect(context, hit,
            BONGO_CAT_UI_CURSOR_POINTER);
        if (hover && nk_input_is_mouse_click_in_rect(&context->input,
            NK_BUTTON_LEFT, hit)) *active = i;
    }
    context->style.window.spacing.y = saved_spacing;
}
