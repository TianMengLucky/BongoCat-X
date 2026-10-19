#include "preferences_about_internal.h"
#include "preferences_state.h"
#include "preferences_about_footer.h"
#include "ui_backend.h"
#include "ui_catime.h"
#include "ui_animation.h"
#include "ui_paint.h"
#include "ui_icons.h"

#include <SDL3/SDL.h>
#include <stdio.h>
static const char *tr(BongoCatPreferences *value, const char *key,
    const char *fallback) {
    return bongo_cat_i18n_get(value->app->i18n, key, fallback);
}
static float width(const struct nk_user_font *font, const char *value) {
    return font->width(font->userdata, font->height, value, nk_strlen(value));
}
static void text(struct nk_command_buffer *canvas, struct nk_rect bounds,
    const char *value, const struct nk_user_font *font, struct nk_color color) {
    nk_draw_text(canvas, bounds, value, nk_strlen(value), font,
        nk_rgba(0, 0, 0, 0), color);
}
static bool hit(struct nk_context *context, struct nk_rect bounds) {
    return !context->current->widgets_disabled &&
        !(context->current->layout->flags & NK_WINDOW_ROM) &&
        nk_input_is_mouse_hovering_rect(&context->input, bounds) &&
        nk_input_is_mouse_hovering_rect(&context->input, nk_window_get_canvas(context)->clip) &&
        nk_input_is_mouse_click_in_rect(&context->input, NK_BUTTON_LEFT, bounds);
}
static void link_cursor(struct nk_context *context, struct nk_rect bounds) {
    if (!context->current->widgets_disabled &&
        !(context->current->layout->flags & NK_WINDOW_ROM) &&
        nk_input_is_mouse_hovering_rect(&context->input, bounds) &&
        nk_input_is_mouse_hovering_rect(&context->input, nk_window_get_canvas(context)->clip))
        bongo_cat_ui_cursor_hover_rect(context, bounds,
            BONGO_CAT_UI_CURSOR_POINTER);
}
static void open_url(const char *url) {
    if (!SDL_OpenURL(url)) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
        "Cannot open URL: %s", SDL_GetError());
}
static void image_contain(struct nk_command_buffer *canvas, unsigned int texture,
    int image_width, int image_height, struct nk_rect bounds) {
    if (!texture || image_width < 1 || image_height < 1) return;
    float scale = NK_MIN(bounds.w / image_width, bounds.h / image_height);
    struct nk_rect target = nk_rect(bounds.x + (bounds.w - image_width * scale) * .5f,
        bounds.y + (bounds.h - image_height * scale) * .5f,
        image_width * scale, image_height * scale);
    struct nk_image image = nk_image_id((int)texture);
    nk_draw_image(canvas, target, &image, nk_rgb(255, 255, 255));
}
static void support_logo(BongoCatPreferences *value,
    struct nk_context *context,
    struct nk_command_buffer *canvas, struct nk_rect bounds,
    BongoCatUIPalette p) {
    bool hover = nk_input_is_mouse_hovering_rect(&context->input, bounds);
    float lift = bongo_cat_ui_animate_eased(context, "support-logo-hover",
        hover ? 1.0f : 0.0f, 250.0f, BONGO_CAT_UI_EASE_SPRING);
    float scale = 1.0f + .04f * lift;
    struct nk_rect raised = nk_rect(
        bounds.x + (bounds.w - bounds.w * scale) * .5f,
        bounds.y - 5.0f * lift + (bounds.h - bounds.h * scale) * .5f,
        bounds.w * scale, bounds.h * scale);
    /* The logo asset is the complete brand mark; avoid wrapping it in a
       second card so the support page and sidebar share the same identity. */
    if (p.effects) bongo_cat_ui_paint_shadow(context, raised, 42, 0, 10, 24, 0,
        nk_rgba(p.pink.r, p.pink.g, p.pink.b, 70));
    image_contain(canvas, value->logo_texture, value->logo_width,
        value->logo_height, raised);
    link_cursor(context, bounds);
    if (hit(context, bounds)) open_url("https://github.com/TianMengLucky/BongoCat-X");
}
static void star_button(BongoCatPreferences *value,
    struct nk_context *context, struct nk_command_buffer *canvas,
    struct nk_rect bounds, BongoCatUIPalette p) {
    bool hover = nk_input_is_mouse_hovering_rect(&context->input, bounds);
    if (hover) bounds.y -= 2;
    if (p.effects) {
        bongo_cat_ui_paint_shadow(context, bounds, 24, 0, 10, 26, 0,
            nk_rgba(p.pink.r, p.pink.g, p.pink.b, hover ? 97 : 77));
        bongo_cat_ui_paint_gradient(context, bounds, 24,
            hover ? nk_rgb(255, 141, 184) : nk_rgb(255, 155, 193),
            hover ? nk_rgb(237, 103, 154) : nk_rgb(244, 119, 168));
    } else nk_fill_rect(canvas, bounds, 23, hover ? p.pink_hover : p.pink);
    nk_stroke_rect(canvas, bounds, 24, 1, nk_rgba(255, 255, 255, 133));
    bongo_cat_preferences_icon_draw(value, canvas,
        BONGO_CAT_UI_ICON_GITHUB,
        nk_rect(bounds.x + 22, bounds.y + 14.5f, 19, 19), p.on_accent);
    text(canvas, nk_rect(bounds.x + 50, bounds.y +
        (bounds.h - value->ui.label_font->height) * .5f,
        bounds.w - 62, value->ui.label_font->height), tr(value,
        "native.support.starOnGitHub", "Star on GitHub!"),
        value->ui.label_font, p.on_accent);
    link_cursor(context, bounds);
    if (hit(context, bounds))
        open_url("https://github.com/TianMengLucky/BongoCat-X");
}
static void hero_title(BongoCatPreferences *value, struct nk_context *context,
    struct nk_command_buffer *canvas, struct nk_rect bounds,
    BongoCatUIPalette p) {
    const char *title = "BongoCat-X";
    const char *by = tr(value, "native.support.by", "by ");
    const char *developer = "vladelaina";
    float gap = 8, title_width = width(value->ui.hero_font, title);
    float by_width = width(value->ui.caption_font, by),
        developer_width = width(value->ui.caption_font, developer);
    float x = bounds.x + (bounds.w - title_width - by_width -
        developer_width - gap) * .5f;
    text(canvas, nk_rect(x, bounds.y, title_width + 1, 32), title,
        value->ui.hero_font, p.text);
    text(canvas, nk_rect(x + title_width + gap, bounds.y + 8,
        by_width + 1, 22), by, value->ui.caption_font, p.muted);
    struct nk_rect link = nk_rect(x + title_width + gap + by_width,
        bounds.y + 8, developer_width + 1, 22);
    bool hover = nk_input_is_mouse_hovering_rect(&context->input, link);
    float amount = bongo_cat_ui_animate_eased(context, "support-author-hover",
        hover ? 1.0f : 0.0f, 200, BONGO_CAT_UI_EASE_STANDARD);
    text(canvas, link, developer, value->ui.caption_font,
        bongo_cat_ui_color_mix(p.accent, p.pink, amount));
    link_cursor(context, link); if (hit(context, link))
        open_url("https://vladelaina.com");
}
void bongo_cat_preferences_about_hero(BongoCatPreferences *value,
    struct nk_context *context) {
    float available = nk_window_get_content_region(context).w;
    const bool narrow = available < 580.0f;
    float shift = narrow ? 56.0f : 0.0f;
    const char *description = tr(value, "native.support.heroText",
        "Thank you for your support. Every use and share helps BongoCat grow.");
    float description_width = NK_MAX(1.0f, available - 72.0f);
    float description_height = NK_MAX(40.0f,
        bongo_cat_about_text_height(description, value->ui.caption_font, description_width));
    float footer_y = 234.0f + shift + description_height + 26.0f;
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    float footer_height = bongo_cat_preferences_about_footer(value, context, NULL,
        nk_rect(0, 0, available, 0), p);
    struct nk_rect bounds;
    nk_layout_row_dynamic(context, footer_y + footer_height + 12.0f, 1);
    if (nk_widget(&bounds, context) == NK_WIDGET_INVALID) return;
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    if (p.effects) {
        nk_push_scissor(canvas, bounds);
        bongo_cat_ui_paint_radial_circle(context,
            nk_rect(bounds.x + bounds.w * .5f - 300,
            bounds.y - 190, 600, 360),
            nk_rgba(p.accent.r, p.accent.g, p.accent.b, 41),
            nk_rgba(p.pink.r, p.pink.g, p.pink.b, 20), .49f, .84f);
        nk_push_scissor(canvas, nk_window_get_content_region(context));
    }
    star_button(value, context, canvas,
        nk_rect(bounds.x + (narrow ? (bounds.w - 177) * .5f : bounds.w - 188),
            bounds.y + 6, 177, 46), p);
    support_logo(value, context, canvas,
        nk_rect(bounds.x + (bounds.w - 168) * .5f + 3,
        bounds.y + 10 + shift, 168, 168), p);
    hero_title(value, context, canvas, nk_rect(bounds.x + 3, bounds.y + 190 + shift,
        bounds.w, 36), p);
    (void)bongo_cat_about_paragraph(context,
        nk_rect(bounds.x + 36, bounds.y + 234 + shift, description_width, description_height),
        description, value->ui.caption_font, p.muted, true, value->ui.caption_font->height + 6.0f);
    bongo_cat_preferences_about_footer(value, context, canvas,
        nk_rect(bounds.x, bounds.y + footer_y, bounds.w, footer_height), p);
}

