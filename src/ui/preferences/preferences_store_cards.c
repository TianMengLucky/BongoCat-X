#include "preferences_state.h"
#include "preferences_model_cover.h"
#include "preferences_model_glyphs.h"
#include "preferences_notice.h"
#include "bongo_cat/store.h"
#include "bongo_cat/i18n.h"
#include "bongo_cat/preferences.h"
#include "ui_paint.h"
#include <stdio.h>
static const char *label(BongoCatApp *app, const char *key, const char *fallback) {
    char full[128]; snprintf(full, sizeof(full), "pages.preference.store.%s", key);
    return bongo_cat_i18n_get(app->i18n, full, fallback);
}
static void centered(struct nk_context *context, struct nk_command_buffer *canvas,
    struct nk_rect rect, const char *text, struct nk_color color) {
    const struct nk_user_font *font = context->style.font;
    float width = font->width(font->userdata, font->height, text, nk_strlen(text));
    rect.x += NK_MAX(0, (rect.w - width) / 2); rect.w = NK_MIN(width, rect.w);
    nk_draw_text(canvas, rect, text, nk_strlen(text), font, nk_rgba(0, 0, 0, 0), color);
}
static void card(BongoCatPreferences *value, struct nk_context *context, unsigned index,
    const BongoCatStoreEntry *entry) {
    struct nk_rect bounds;
    if (nk_widget(&bounds, context) == NK_WIDGET_INVALID) return;
    BongoCatApp *app = value->app; BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    nk_fill_rect(canvas, bounds, 12, p.surface);
    nk_stroke_rect(canvas, bounds, 12, 1, p.border_subtle);
    struct nk_rect preview = nk_rect(bounds.x + 10, bounds.y + 10, bounds.w - 20, 160);
    nk_fill_rect(canvas, preview, 8, p.hover);
    char path[BONGO_CAT_PATH_CAP];
    float raster = value->ui.raster_scale > 0 ? value->ui.raster_scale : 1;
    const BongoCatModelCover *cover = bongo_cat_store_preview(app, index, path) ?
        bongo_cat_preferences_cover_path(app, path, (int)(preview.w * raster),
            (int)(preview.h * raster)) : NULL;
    if (cover) {
        float scale = NK_MIN(preview.w / cover->width, preview.h / cover->height);
        struct nk_rect image = nk_rect(preview.x + (preview.w - cover->width * scale) / 2,
            preview.y + (preview.h - cover->height * scale) / 2, cover->width * scale, cover->height * scale);
        struct nk_image texture = nk_image_id((int)cover->texture);
        nk_draw_image(canvas, image, &texture, nk_rgb(255, 255, 255));
    } else centered(context, canvas, nk_rect(preview.x, preview.y + 70, preview.w, 25),
        label(app, "preview", "Model preview"), p.muted);
    centered(context, canvas, nk_rect(bounds.x + 10, bounds.y + 178, bounds.w - 20, 25), entry->name, p.text);
    struct nk_rect author = nk_rect(bounds.x + 10, bounds.y + 207, bounds.w - 20, 25);
    centered(context, canvas, author, entry->author, entry->author_url[0] ? p.accent : p.muted);
    if (entry->author_url[0] && nk_input_is_mouse_click_in_rect(&context->input, NK_BUTTON_LEFT, author)) {
        if (SDL_strncmp(entry->author_url, "https://", 8) || !SDL_OpenURL(entry->author_url))
            bongo_cat_preferences_notice_show(app, label(app, "failed", "Unable to update the store"), true);
    }
    unsigned operation_index; uint64_t received, total;
    unsigned operation = bongo_cat_store_operation(app, &operation_index, &received, &total);
    bool busy = operation == 1 || operation == 2;
    bool current = index == operation_index;
    bool installed = bongo_cat_store_installed(app, entry);
    const char *action = label(app, installed ? "uninstall" : "download", installed ? "Uninstall" : "Download");
    if (busy && current) action = label(app, operation == 2 ? "installing" : "downloading",
        operation == 2 ? "Installing..." : "Downloading...");
    struct nk_rect button = nk_rect(bounds.x + 12, bounds.y + 242, bounds.w - 24, 34);
    bool hover = nk_input_is_mouse_hovering_rect(&context->input, button);
    nk_fill_rect(canvas, button, 7, busy ? p.hover : hover ? nk_rgb(29, 145, 75) : nk_rgb(35, 165, 86));
    centered(context, canvas, nk_rect(button.x, button.y + 5, button.w, 25), action, busy ? p.muted : nk_rgb(255,255,255));
    if (busy && current && total) {
        float fraction = NK_MIN(1.0f, (float)received / (float)total);
        nk_fill_rect(canvas, nk_rect(button.x, button.y + button.h - 3, button.w * fraction, 3), 1, p.accent);
    }
    if (!busy && nk_input_is_mouse_click_in_rect(&context->input, NK_BUTTON_LEFT, button)) {
        BongoCatError error = {0};
        bool ok = installed ? bongo_cat_store_uninstall(app, entry, &error) : bongo_cat_store_download(app, index);
        if (!ok) bongo_cat_preferences_notice_show(app, label(app, "failed", "Unable to update the store"), true);
        value->render_dirty = true;
    }
    if (current && (operation == 4 || operation == 5))
        centered(context, canvas, nk_rect(bounds.x + 10, bounds.y + 283, bounds.w - 20, 25),
            label(app, operation == 5 ? "hashMismatch" : "failed",
                operation == 5 ? "SHA-256 mismatch; download rejected" : "Unable to update the store"), p.accent);
}
void bongo_cat_preferences_store_cards(BongoCatPreferences *value, struct nk_context *context, unsigned count) {
    BongoCatApp *app = value->app;
    uint64_t hash = UINT64_C(14695981039346656037); bool missing = false;
    for (unsigned i = 0; i < count; ++i) {
        BongoCatStoreEntry entry; if (!bongo_cat_store_entry(app, i, &entry)) continue;
        missing |= !bongo_cat_preferences_model_glyphs_ready(value, entry.name) ||
            !bongo_cat_preferences_model_glyphs_ready(value, entry.author);
        const char *texts[] = {entry.name, entry.author};
        for (unsigned n = 0; n < 2; ++n)
            for (const unsigned char *s = (const unsigned char *)texts[n]; *s; ++s)
                hash = (hash ^ *s) * UINT64_C(1099511628211);
    }
    if (missing && value->store_glyph_revision != hash) {
        value->store_glyph_revision = hash; value->font_reload_pending = true;
    }
    bongo_cat_preferences_model_covers_begin(app);
    int columns = NK_MAX(1, (int)(nk_window_get_content_region(context).w / 245));
    nk_layout_row_dynamic(context, 318, columns);
    for (unsigned i = 0; i < count; ++i) {
        BongoCatStoreEntry entry; if (bongo_cat_store_entry(app, i, &entry)) card(value, context, i, &entry);
    }
    bongo_cat_preferences_model_covers_prune(app);
}
