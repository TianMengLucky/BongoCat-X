#include "plugin_ui_internal.h"
#include "preferences_widgets.h"
#include "preferences_controls.h"
#include "preferences_theme.h"
#include "preferences_number_edit.h"
#include "preferences_model_cover.h"
#include "preferences_model_glyphs.h"
#include "preferences_notice.h"
#include "bongo_cat/preferences.h"
#include "ui_paint.h"
#include <stdio.h>
#include <string.h>

static struct nk_rect rect(BongoCatPluginUiRect r) { return nk_rect(r.x, r.y, r.w, r.h); }
static struct nk_color color(uint32_t c) {
    return nk_rgba(c & 255u, (c >> 8) & 255u, (c >> 16) & 255u, c >> 24);
}
static const struct nk_user_font *font(BongoCatPluginUi *ui, uint32_t kind) {
    BongoCatUIBackend *backend = &ui->preferences->ui;
    const struct nk_user_font *result = kind == BONGO_UI_CAPTION ? backend->caption_font :
        kind == BONGO_UI_LABEL ? backend->label_font :
        kind == BONGO_UI_HEADING ? backend->heading_font : backend->body_font;
    return result ? result : ui->context->style.font;
}
static void section(BongoCatPluginUi *ui, const char *text) { bongo_cat_pref_section(ui->context, text); }
static void status(BongoCatPluginUi *ui, const char *id, const char *title, const char *text) {
    bongo_cat_pref_status(ui->context, id, title, text);
}
static uint32_t segments(BongoCatPluginUi *ui, const char *id,
    const char *const *labels, uint32_t count, uint32_t selected) {
    if (!labels || !count || count > 32) return selected;
    return (uint32_t)bongo_cat_pref_segments(ui->context, id, labels,
        (int)count, (int)(selected < count ? selected : 0));
}
static uint32_t edit(BongoCatPluginUi *ui, const char *id, char *text,
    uint32_t capacity, uint32_t ascii) {
    if (!text || !capacity || capacity > 4096) return 0;
    text[capacity - 1] = 0;
    nk_flags flags = nk_edit_string_zero_terminated(ui->context, NK_EDIT_FIELD,
        text, (int)capacity, ascii ? nk_filter_ascii : nk_filter_default);
    if (flags & NK_EDIT_ACTIVATED) SDL_StartTextInput(ui->preferences->window);
    if (flags & NK_EDIT_ACTIVE) ui->preferences->render_dirty = true;
    if (!ascii && id) {
        BongoCatPreferences *value = ui->preferences;
        for (size_t i = 0; i < SDL_arraysize(value->plugin_edit_ids); ++i) {
            if (!value->plugin_edit_ids[i][0] || !strcmp(value->plugin_edit_ids[i], id)) {
                SDL_strlcpy(value->plugin_edit_ids[i], id, sizeof(value->plugin_edit_ids[i]));
                SDL_strlcpy(value->plugin_edit_texts[i], text, sizeof(value->plugin_edit_texts[i]));
                break;
            }
        }
    }
    return (flags & NK_EDIT_ACTIVE) != 0;
}
static void number(BongoCatPluginUi *ui, const char *id, const char *title,
    double *value, double minimum, double maximum) {
    if (!value) return;
    struct nk_context *ctx = ui->context;
    nk_layout_row_dynamic(ctx, 30, 1); nk_label(ctx, title, NK_TEXT_LEFT);
    nk_layout_row_dynamic(ctx, 32, 1);
    struct nk_rect bounds;
    if (nk_widget(&bounds, ctx) == NK_WIDGET_INVALID) return;
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(ctx));
    nk_fill_rect(nk_window_get_canvas(ctx), bounds, 6, p.field);
    nk_stroke_rect(nk_window_get_canvas(ctx), bounds, 6, 1, p.border_subtle);
    char text[64]; snprintf(text, sizeof(text), "%.9f", *value);
    char *end = text + strlen(text) - 1;
    while (*end == '0') *end-- = 0;
    if (*end == '.') *end = 0;
    bounds.x += 8; bounds.w -= 16;
    if (!bongo_cat_pref_number_edit(ctx, id, bounds, text, false, minimum, maximum, value))
        nk_draw_text(nk_window_get_canvas(ctx), bounds, text, nk_strlen(text),
            ctx->style.font, nk_rgba(0, 0, 0, 0), p.text);
}
static void disabled(BongoCatPluginUi *ui, uint32_t begin) {
    if (begin) nk_widget_disable_begin(ui->context);
    else nk_widget_disable_end(ui->context);
}
static uint32_t region(BongoCatPluginUi *ui, BongoCatPluginUiRect *out) {
    if (!out) return 0;
    struct nk_rect r = nk_rect(0, 0, 0, 0);
    enum nk_widget_layout_states state = nk_widget(&r, ui->context);
    *out = (BongoCatPluginUiRect){r.x, r.y, r.w, r.h};
    if (state == NK_WIDGET_INVALID) return 0;
    return state == NK_WIDGET_VALID &&
        !(ui->context->current->layout->flags & NK_WINDOW_ROM) ? 2u : 1u;
}
static void fill_rect(BongoCatPluginUi *ui, BongoCatPluginUiRect r, float rounding, uint32_t c) {
    nk_fill_rect(nk_window_get_canvas(ui->context), rect(r), rounding, color(c));
}
static void stroke_rect(BongoCatPluginUi *ui, BongoCatPluginUiRect r, float rounding, float width, uint32_t c) {
    nk_stroke_rect(nk_window_get_canvas(ui->context), rect(r), rounding, width, color(c));
}
static void circle(BongoCatPluginUi *ui, BongoCatPluginUiRect r, uint32_t c) {
    nk_fill_circle(nk_window_get_canvas(ui->context), rect(r), color(c));
}
static void line(BongoCatPluginUi *ui, BongoCatPluginUiRect r, float width, uint32_t c) {
    nk_stroke_line(nk_window_get_canvas(ui->context), r.x, r.y, r.x + r.w, r.y + r.h, width, color(c));
}
static void draw_text(BongoCatPluginUi *ui, BongoCatPluginUiRect r, const char *text,
    uint32_t type, uint32_t c, uint32_t align) {
    if (!text) return;
    const struct nk_user_font *f = font(ui, type);
    float width = f->width(f->userdata, f->height, text, nk_strlen(text));
    if (align == 1) r.x += NK_MAX(0, (r.w - width) * .5f);
    else if (align == 2) r.x += NK_MAX(0, r.w - width);
    r.w = NK_MIN(r.w, width + 1);
    r.y += (r.h - f->height) * .5f; r.h = f->height;
    nk_draw_text(nk_window_get_canvas(ui->context), rect(r), text, nk_strlen(text),
        f, nk_rgba(0, 0, 0, 0), color(c));
}
static float measure(BongoCatPluginUi *ui, const char *text, uint32_t type) {
    const struct nk_user_font *f = font(ui, type);
    return f->width(f->userdata, f->height, text, nk_strlen(text));
}
static uint32_t input(BongoCatPluginUi *ui, BongoCatPluginUiRect r, uint32_t kind) {
    if (kind > BONGO_UI_POINTER) return 0;
    struct nk_context *ctx = ui->context;
    if (!nk_input_is_mouse_hovering_rect(&ctx->input, nk_window_get_canvas(ctx)->clip) ||
        !nk_input_is_mouse_hovering_rect(&ctx->input, rect(r))) return 0;
    if (kind == BONGO_UI_HOVERED) return 1;
    if (ctx->current->layout->flags & NK_WINDOW_ROM) return 0;
    if (kind == BONGO_UI_POINTER) {
        bongo_cat_ui_cursor_hover_rect(ctx, rect(r), BONGO_CAT_UI_CURSOR_POINTER);
        return 1;
    }
    return kind == BONGO_UI_CLICKED ?
        nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_LEFT, rect(r)) :
        nk_input_is_mouse_down(&ctx->input, NK_BUTTON_LEFT);
}
static void pointer(BongoCatPluginUi *ui, float *x, float *y) {
    if (x) *x = ui->context->input.mouse.pos.x;
    if (y) *y = ui->context->input.mouse.pos.y;
}
static void tooltip(BongoCatPluginUi *ui, const char *text) { nk_tooltip(ui->context, text); }
static uint32_t palette(BongoCatPluginUi *ui, uint32_t role) {
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(ui->context));
    const struct nk_color colors[] = {p.text, p.muted, p.surface, p.field, p.hover,
        p.selection, p.border, p.border_subtle, p.accent, p.accent_hover, p.danger,
        p.danger_background, p.link, p.link_hover};
    struct nk_color c = colors[role < SDL_arraysize(colors) ? role : 0];
    return (uint32_t)c.r | (uint32_t)c.g << 8 | (uint32_t)c.b << 16 | (uint32_t)c.a << 24;
}
static float width(BongoCatPluginUi *ui) { return nk_window_get_content_region(ui->context).w; }
static float spacing(BongoCatPluginUi *ui) { return ui->context->style.window.spacing.x; }
static uint32_t button(BongoCatPluginUi *ui, const char *id, const char *text, uint32_t kind) {
    (void)id;
    struct nk_context *ctx = ui->context;
    if (!kind) return nk_button_label(ctx, text) != 0;
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(ctx));
    struct nk_style_button s = ctx->style.button;
    s.normal = nk_style_item_color(p.field);
    s.hover = s.active = nk_style_item_color(kind == BONGO_UI_BUTTON_DANGER ? p.danger_background : p.hover);
    s.text_normal = s.text_hover = s.text_active = kind == BONGO_UI_BUTTON_DANGER ? p.danger : p.link;
    if (kind == BONGO_UI_BUTTON_SUCCESS) {
        s.normal = nk_style_item_color(nk_rgb(35, 165, 86));
        s.hover = s.active = nk_style_item_color(nk_rgb(29, 145, 75));
        s.text_normal = s.text_hover = s.text_active = nk_rgb(255, 255, 255);
    }
    if (kind == BONGO_UI_BUTTON_LINK) nk_style_push_font(ctx, font(ui, BONGO_UI_LABEL));
    bool clicked = nk_button_label_styled(ctx, &s, text) != 0;
    if (kind == BONGO_UI_BUTTON_LINK) nk_style_pop_font(ctx);
    return clicked;
}
static uint32_t tree_begin(BongoCatPluginUi *ui, const char *id, const char *title, uint32_t *open) {
    if (!open) return 0;
    enum nk_collapse_states state = *open ? NK_MAXIMIZED : NK_MINIMIZED;
    bool result = nk_tree_state_push(ui->context, NK_TREE_TAB, title, &state) != 0;
    (void)id;
    *open = state == NK_MAXIMIZED;
    return result;
}
static void tree_end(BongoCatPluginUi *ui) { nk_tree_state_pop(ui->context); }
static void progress(BongoCatPluginUi *ui, float fraction) {
    nk_size amount = (nk_size)(NK_CLAMP(0.0f, fraction, 1.0f) * 1000);
    nk_progress(ui->context, &amount, 1000, NK_FIXED);
}
static uint32_t image(BongoCatPluginUi *ui, const char *path, BongoCatPluginUiRect r) {
    if (!(r.w > 0 && r.w <= 8192 && r.h > 0 && r.h <= 8192)) return 0;
    float raster = ui->preferences->ui.raster_scale;
    if (raster <= 0) raster = 1;
    const BongoCatModelCover *cover = bongo_cat_preferences_cover_path_bounded(ui->preferences->app,
        path, (int)(r.w * raster), (int)(r.h * raster));
    if (!cover || cover->width <= 0 || cover->height <= 0) return 0;
    float scale = NK_MIN(r.w / cover->width, r.h / cover->height);
    struct nk_rect bounds = nk_rect(r.x + (r.w - cover->width * scale) / 2,
        r.y + (r.h - cover->height * scale) / 2, cover->width * scale, cover->height * scale);
    struct nk_image texture = nk_image_id((int)cover->texture);
    nk_draw_image(nk_window_get_canvas(ui->context), bounds, &texture, nk_rgb(255, 255, 255));
    return 1;
}
static void images_begin(BongoCatPluginUi *ui) { bongo_cat_preferences_model_covers_begin(ui->preferences->app); }
static void images_end(BongoCatPluginUi *ui) { bongo_cat_preferences_model_covers_prune(ui->preferences->app); }
static void glyphs(BongoCatPluginUi *ui, const char *const *texts, uint32_t count, uint64_t *previous) {
    if (!texts || !previous || count > 2048) return;
    uint64_t hash = UINT64_C(14695981039346656037); bool missing = false;
    for (uint32_t i = 0; i < count; ++i) {
        if (!texts[i]) continue;
        if (!bongo_cat_preferences_model_glyphs_ready(ui->preferences, texts[i])) missing = true;
        for (const unsigned char *s = (const unsigned char *)texts[i]; *s; ++s)
            hash = (hash ^ *s) * UINT64_C(1099511628211);
    }
    if (missing && *previous != hash) { *previous = hash; ui->preferences->font_reload_pending = true; }
}
static void notice(BongoCatPluginUi *ui, const char *text, uint32_t error) {
    bongo_cat_preferences_notice_show(ui->preferences->app, text, error != 0);
}
static void invalidate(BongoCatPluginUi *ui) { ui->preferences->render_dirty = true; }
static void unfocus(BongoCatPluginUi *ui) {
    nk_edit_unfocus(ui->context); bongo_cat_pref_number_edit_reset(ui->context);
}
const BongoCatPluginPageUiHost *bongo_cat_preferences_plugin_canvas(void) {
    static BongoCatPluginPageUiHost host = {
        BONGO_CAT_PLUGIN_PAGE_UI_ABI, sizeof(BongoCatPluginPageUiHost), NULL,
        section, status, segments, edit, number, disabled, region, fill_rect,
        stroke_rect, circle, line, draw_text, measure, input, pointer, tooltip,
        palette, width, spacing, button, tree_begin, tree_end, progress, image,
        images_begin, images_end, glyphs, notice, invalidate, unfocus
    };
    host.widgets = bongo_cat_preferences_plugin_widgets();
    return &host;
}
