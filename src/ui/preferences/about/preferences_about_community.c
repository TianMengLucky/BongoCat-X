#include "preferences_about_internal.h"
#include "preferences_about_community.h"
#include "preferences_state.h"
#include "ui_animation.h"
#include "ui_backend.h"
#include "ui_catime.h"
#include "ui_icons.h"
#include "ui_paint.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

static bool hit(struct nk_context *context, struct nk_rect bounds) {
    return nk_input_is_mouse_hovering_rect(&context->input, bounds) &&
        nk_input_is_mouse_click_in_rect(&context->input, NK_BUTTON_LEFT, bounds);
}

static const char *tr(BongoCatPreferences *value, const char *key,
    const char *fallback) {
    return bongo_cat_i18n_get(value->app->i18n, key, fallback);
}

static void text(struct nk_command_buffer *canvas, struct nk_rect bounds,
    const char *value, const struct nk_user_font *font, struct nk_color color) {
    nk_draw_text(canvas, bounds, value, nk_strlen(value), font,
        nk_rgba(0, 0, 0, 0), color);
}

static void centered_span(struct nk_command_buffer *canvas,
    struct nk_rect bounds, const char *value, int length,
    const struct nk_user_font *font, struct nk_color color) {
    float width = font->width(font->userdata, font->height,
        value, length);
    nk_draw_text(canvas, nk_rect(bounds.x + (bounds.w - width) * .5f,
        bounds.y + (bounds.h - font->height) * .5f,
        NK_MIN(width + 1, bounds.w), font->height), value, length, font,
        nk_rgba(0, 0, 0, 0), color);
}

static void centered(struct nk_command_buffer *canvas, struct nk_rect bounds,
    const char *value, const struct nk_user_font *font, struct nk_color color) {
    centered_span(canvas, bounds, value, nk_strlen(value), font, color);
}

static float span_width(const struct nk_user_font *font,
    const char *value, int length) {
    return font->width(font->userdata, font->height, value, length);
}

static void span(struct nk_command_buffer *canvas, struct nk_rect bounds,
    const char *value, int length, const struct nk_user_font *font,
    struct nk_color color) {
    if (length > 0) nk_draw_text(canvas, bounds, value, length, font,
        nk_rgba(0, 0, 0, 0), color);
}

void bongo_cat_preferences_about_projects_heading(
    BongoCatPreferences *value, struct nk_context *context,
    struct nk_rect bounds) {
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    centered(canvas, nk_rect(bounds.x, bounds.y + 20, bounds.w, 30),
        tr(value, "native.support.works", "More apps"),
        value->ui.heading_font, p.text);
    const char *caption = tr(value, "native.support.worksText",
        "More software from vladelaina");
    static const char developer_name[] = "vladelaina";
    const char *developer = strstr(caption, developer_name);
    int prefix = developer ? (int)(developer - caption) : nk_strlen(caption);
    int name_length = developer ? (int)(sizeof(developer_name) - 1) : 0;
    const char *suffix = developer ? developer + name_length : caption + prefix;
    int suffix_length = nk_strlen(suffix);
    float prefix_width = span_width(value->ui.caption_font, caption, prefix);
    float name_width = span_width(value->ui.caption_font,
        developer ? developer : "", name_length);
    float suffix_width = span_width(value->ui.caption_font, suffix, suffix_length);
    float x = bounds.x + (bounds.w - prefix_width - name_width - suffix_width) * .5f;
    float y = bounds.y + 49;
    span(canvas, nk_rect(x, y, prefix_width + 1, 24), caption, prefix,
        value->ui.caption_font, p.muted);
    struct nk_rect link = nk_rect(x + prefix_width, y, name_width + 1, 24);
    bool hover = developer && nk_input_is_mouse_hovering_rect(
        &context->input, link);
    float amount = bongo_cat_ui_animate_eased(context, "works-author-hover",
        hover ? 1.0f : 0.0f, 200, BONGO_CAT_UI_EASE_STANDARD);
    span(canvas, link, developer ? developer : "", name_length,
        value->ui.caption_font, bongo_cat_ui_color_mix(p.accent, p.pink, amount));
    span(canvas, nk_rect(link.x + name_width, y, suffix_width + 1, 24),
        suffix, suffix_length, value->ui.caption_font, p.muted);
    if (hover) bongo_cat_ui_cursor_hover_rect(context, link,
        BONGO_CAT_UI_CURSOR_POINTER);
    if (developer && hit(context, link))
        SDL_OpenURL("https://vladelaina.com");
}
