#include "preferences_state.h"
#include "preferences_widgets.h"
#include "preferences_controls.h"
#include "preferences_notice.h"
#include "preferences_model_glyphs.h"
#include "bongo_cat/items.h"
#include "bongo_cat/i18n.h"
#include "bongo_cat/path.h"
#include "bongo_cat/platform.h"
#include "bongo_cat/preferences.h"
#include "ui_paint.h"
#include "ui_icons.h"
#include <stdio.h>
#include <string.h>
static const char *label(BongoCatApp *app, const char *key, const char *fallback) {
    char full[128]; snprintf(full, sizeof(full), "pages.preference.items.%s", key);
    return bongo_cat_i18n_get(app->i18n, full, fallback);
}
static void notice(BongoCatApp *app, bool ok, const BongoCatError *error) {
    char message[1024];
    snprintf(message, sizeof(message), "%s%s%s", label(app, ok ? "saved" : "failed",
        ok ? "Items saved" : "Unable to update items"),
        !ok && error && error->message[0] ? "\n" : "",
        !ok && error ? error->message : "");
    bongo_cat_preferences_notice_show(app, message, !ok);
    bongo_cat_preferences_invalidate(app->preferences);
}
void bongo_cat_preferences_items_import(BongoCatApp *app, const char *path) {
    BongoCatError error = {0};
    bool ok = bongo_cat_items_queue_import(app, path);
    if (ok && app->preferences) {
        BongoCatItemScene scene;
        if (bongo_cat_items_snapshot(app, &scene)) app->preferences->selected_item = (int)scene.count - 1;
    }
    if (!ok) notice(app, false, &error);
}
static bool item_action(BongoCatPreferences *value, struct nk_context *context,
    struct nk_rect bounds, bool duplicate, bool enabled) {
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    bool hover = nk_input_is_mouse_hovering_rect(&context->input, bounds);
    struct nk_color color = !enabled ? p.muted : duplicate ? p.accent : p.danger;
    if (hover && enabled) nk_fill_rect(canvas, bounds, 8, duplicate ? p.selection : p.danger_background);
    struct nk_rect glyph = nk_rect(bounds.x + 7, bounds.y + 7, 18, 18);
    if (duplicate) {
        nk_stroke_rect(canvas, nk_rect(glyph.x + 5, glyph.y, 12, 12), 2, 1.5f, color);
        nk_fill_rect(canvas, nk_rect(glyph.x, glyph.y + 5, 12, 12), 2, p.surface);
        nk_stroke_rect(canvas, nk_rect(glyph.x, glyph.y + 5, 12, 12), 2, 1.5f, color);
    } else bongo_cat_preferences_icon_draw(value, canvas, BONGO_CAT_UI_ICON_TRASH, glyph, color);
    if (hover) {
        nk_tooltip(context, label(value->app, duplicate ? "duplicate" : "remove",
            duplicate ? "Duplicate" : "Remove from scene"));
        if (enabled) bongo_cat_ui_cursor_hover_rect(context, bounds, BONGO_CAT_UI_CURSOR_POINTER);
    }
    return enabled && hover && nk_input_is_mouse_click_in_rect(&context->input, NK_BUTTON_LEFT, bounds);
}
static bool item_row(BongoCatPreferences *value, struct nk_context *context,
    const BongoCatItemScene *scene, uint32_t index) {
    struct nk_rect bounds;
    nk_layout_row_dynamic(context, 40, 1);
    enum nk_widget_layout_states state = nk_widget(&bounds, context);
    if (state == NK_WIDGET_INVALID) return false;
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    bool selected = value->selected_item == (int)index;
    bool hover = nk_input_is_mouse_hovering_rect(&context->input, bounds);
    nk_fill_rect(canvas, bounds, 10, selected ? p.selection : hover ? p.hover : p.surface);
    nk_stroke_rect(canvas, bounds, 10, 1, selected ? p.border : p.border_subtle);
    struct nk_rect remove = nk_rect(bounds.x + bounds.w - 38, bounds.y + 4, 32, 32);
    struct nk_rect duplicate = nk_rect(remove.x - 38, remove.y, 32, 32);
    struct nk_rect title = nk_rect(bounds.x + 12, bounds.y, NK_MAX(1.0f, bounds.w - 98), bounds.h);
    const struct nk_user_font *font = bongo_cat_ui_body_font(context);
    nk_draw_text(canvas, nk_rect(title.x, title.y + (title.h - font->height) * .5f,
        title.w, font->height), scene->items[index].name, nk_strlen(scene->items[index].name),
        font, nk_rgba(0, 0, 0, 0), p.text);
    bool interactive = state == NK_WIDGET_VALID && !(context->current->layout->flags & NK_WINDOW_ROM);
    bool ready = interactive && !bongo_cat_items_busy(value->app);
    if (item_action(value, context, duplicate, true, ready && scene->count < BONGO_CAT_ITEMS_CAP)) {
        bool ok = bongo_cat_items_duplicate(value->app, index);
        if (ok) value->selected_item = (int)scene->count;
        else notice(value->app, false, NULL);
        value->render_dirty = true;
        return ok;
    }
    if (item_action(value, context, remove, false, ready)) {
        bool ok = bongo_cat_items_remove(value->app, index);
        if (ok) value->selected_item = NK_CLAMP(0, value->selected_item, (int)scene->count - 2);
        else notice(value->app, false, NULL);
        value->render_dirty = true;
        return ok;
    }
    if (interactive && nk_input_is_mouse_click_in_rect(&context->input, NK_BUTTON_LEFT, title)) {
        value->selected_item = (int)index;
        value->render_dirty = true;
    }
    if (nk_input_is_mouse_hovering_rect(&context->input, title)) {
        if (font->width(font->userdata, font->height, scene->items[index].name,
            nk_strlen(scene->items[index].name)) > title.w) nk_tooltip(context, scene->items[index].name);
        if (interactive) bongo_cat_ui_cursor_hover_rect(context, title, BONGO_CAT_UI_CURSOR_POINTER);
    }
    return false;
}
static void draw_preview(BongoCatPreferences *value, struct nk_context *context, BongoCatItem *item) {
    /* An interactive positioning pad uses the same normalized coordinates as
       the renderer. Numeric fields provide precise placement and rotation. */
    nk_layout_row_dynamic(context, 150, 1);
    struct nk_rect bounds;
    if (nk_widget(&bounds, context) == NK_WIDGET_INVALID) return;
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    nk_fill_rect(canvas, bounds, 12, p.hover);
    nk_stroke_rect(canvas, bounds, 12, 1, p.border);
    nk_stroke_line(canvas, bounds.x + bounds.w / 2, bounds.y,
        bounds.x + bounds.w / 2, bounds.y + bounds.h, 1, p.border_subtle);
    nk_stroke_line(canvas, bounds.x, bounds.y + bounds.h / 2,
        bounds.x + bounds.w, bounds.y + bounds.h / 2, 1, p.border_subtle);
    if (!item->locked && nk_input_is_mouse_hovering_rect(&context->input, bounds) &&
        nk_input_is_mouse_down(&context->input, NK_BUTTON_LEFT)) {
        item->x = NK_CLAMP(0.0f, (context->input.mouse.pos.x - bounds.x) / bounds.w * 100, 100.0f);
        item->y = NK_CLAMP(0.0f, (context->input.mouse.pos.y - bounds.y) / bounds.h * 100, 100.0f);
        value->render_dirty = true;
    }
    float cx = bounds.x + bounds.w * item->x / 100, cy = bounds.y + bounds.h * item->y / 100;
    nk_fill_circle(canvas, nk_rect(cx - 7, cy - 7, 14, 14), p.accent);
}
void bongo_cat_preferences_page_items(BongoCatPreferences *value, struct nk_context *context) {
    BongoCatApp *app = value->app; BongoCatItemScene scene;
    if (!bongo_cat_items_snapshot(app, &scene)) {
        nk_layout_row_dynamic(context, 45, 1);
        nk_label_wrap(context, label(app, "failed", "Unable to update items")); return;
    }
    bongo_cat_pref_intro(context, label(app, "help", "Import images and arrange props around your pet. Drag the positioning pad or use the controls. Up to 16 items; GIF uses its first frame."));
    nk_layout_row_dynamic(context, 36, 3);
    if (nk_button_label(context, label(app, "add", "Add item")))
        (void)bongo_cat_preferences_import_items_open(value->import_dialog, value->window);
    if (nk_button_label(context, label(app, "save", "Save scene"))) {
        BongoCatError error = {0}; bool ok = bongo_cat_items_queue_save(app); if (!ok) notice(app, false, &error);
    }
    if (nk_button_label(context, label(app, "folder", "Open items folder"))) {
        char directory[BONGO_CAT_PATH_CAP];
        bool ok = bongo_cat_path_join(directory, sizeof(directory), app->data_root, "items") &&
            bongo_cat_path_create_directory(directory) && bongo_cat_platform_open_directory(directory);
        if (!ok) notice(app, false, NULL);
    }
    if (!scene.count) {
        nk_layout_row_dynamic(context, 70, 1);
        nk_label_wrap(context, label(app, "empty", "No items yet. Add an image or drop it onto this page.")); return;
    }
    value->selected_item = NK_CLAMP(0, value->selected_item, (int)scene.count - 1);
    bongo_cat_pref_section(context, label(app, "list", "Scene items"));
    uint64_t glyph_hash = UINT64_C(14695981039346656037);
    bool missing_glyphs = false;
    for (uint32_t i = 0; i < scene.count; ++i) {
        if (!bongo_cat_preferences_model_glyphs_ready(value, scene.items[i].name)) missing_glyphs = true;
        for (const unsigned char *p = (const unsigned char *)scene.items[i].name; *p; ++p)
            glyph_hash = (glyph_hash ^ *p) * UINT64_C(1099511628211);
        if (item_row(value, context, &scene, i)) return;
    }
    if (missing_glyphs && glyph_hash != value->item_glyph_hash) {
        value->item_glyph_hash = glyph_hash;
        value->font_reload_pending = true;
    }
    bool busy = bongo_cat_items_busy(app);
    if (busy) nk_widget_disable_begin(context);
    BongoCatItem original = scene.items[value->selected_item], item = original;
    bongo_cat_pref_section(context, label(app, "edit", "Selected item"));
    bongo_cat_preferences_items_preview(value, context, item.asset);
    bool visible = item.visible != 0, locked = item.locked != 0, follow = item.follow_model != 0;
    bongo_cat_pref_toggle(context, "item-visible", label(app, "visible", "Visible"), "", &visible);
    bongo_cat_pref_toggle(context, "item-locked", label(app, "locked", "Lock placement"),
        label(app, "lockHint", "Prevent accidental placement changes."), &locked);
    item.visible = visible ? 1u : 0u; item.locked = locked ? 1u : 0u;
    bongo_cat_pref_toggle(context, "item-follow", label(app, "follow", "Follow model viewport"),
        label(app, "followHint", "Follow the model canvas, mirror and vertical flip. This does not attach to an animated mesh."), &follow);
    item.follow_model = follow ? 1u : 0u;
    draw_preview(value, context, &item);
    if (item.locked) nk_widget_disable_begin(context);
    bongo_cat_pref_slider(context, "item-x", label(app, "x", "Horizontal position (%)"), "", 0, &item.x, 100, 1, 50);
    bongo_cat_pref_slider(context, "item-y", label(app, "y", "Vertical position (%)"), "", 0, &item.y, 100, 1, 50);
    bongo_cat_pref_slider(context, "item-scale", label(app, "scale", "Size (%)"),
        label(app, "scaleHint", "Percentage of the shorter viewport edge, preserving proportions."), 1, &item.scale, 200, 1, 25);
    bongo_cat_pref_slider_suffix(context, "item-rotation", label(app, "rotation", "Rotation"), "", -180, &item.rotation, 180, 1, 0, "°");
    if (item.locked) nk_widget_disable_end(context);
    bongo_cat_pref_slider(context, "item-opacity", label(app, "opacity", "Opacity (%)"), "", 0, &item.opacity, 100, 1, 100);
    int layer = item.layer;
    bongo_cat_pref_int(context, "item-layer", label(app, "layer", "Layer"),
        label(app, "layerHint", "Negative layers are behind the model; zero and positive layers are in front. Higher layers draw last."), -30, &layer, 30, 1, 0);
    item.layer = layer;
    if (memcmp(&item, &original, sizeof(item))) (void)bongo_cat_items_update(app, (uint32_t)value->selected_item, &item);
    if (busy) nk_widget_disable_end(context);
}
void bongo_cat_preferences_items_plugin_card(BongoCatPreferences *value, struct nk_context *context) {
    BongoCatApp *app = value->app; BongoCatItemsInfo info;
    bongo_cat_items_info(app, &info);
    struct nk_style_window saved;
    if (!bongo_cat_pref_card_begin(context, "items-plugin", &saved)) return;
    nk_layout_row_dynamic(context, 30, 1); nk_label(context, label(app, "title", "Items"), NK_TEXT_LEFT);
    nk_layout_row_dynamic(context, 76, 1);
    nk_label_wrap(context, label(app, "pluginHint", "Image props with independent placement, layers and saved scenes. Disabling hides the page and props while preserving data."));
    nk_layout_row_dynamic(context, 25, 1);
    nk_label(context, bongo_cat_i18n_get(app->i18n, info.active ? "pages.preference.plugins.active" :
        !info.installed ? "pages.preference.plugins.missing" : "pages.preference.plugins.disabled",
        info.active ? "In use" : !info.installed ? "Not installed" : "Disabled"), NK_TEXT_LEFT);
    nk_layout_row_dynamic(context, 30, 2);
    nk_label(context, bongo_cat_i18n_get(app->i18n, "pages.preference.plugins.enabled", "Enabled"), NK_TEXT_LEFT);
    bool enabled = info.enabled;
    if (bongo_cat_pref_control_toggle_available(context, "enable-items-plugin", &enabled, info.installed && !bongo_cat_items_busy(app))) {
        BongoCatError error = {0}; bool ok = bongo_cat_items_set_enabled(app, enabled, &error); notice(app, ok, &error);
    }
    nk_layout_row_dynamic(context, 30, 1);
    if (info.managed && !bongo_cat_items_busy(app) && nk_button_label(context, bongo_cat_i18n_get(app->i18n, "pages.preference.plugins.remove", "Remove plugin"))) {
        BongoCatError error = {0}; bool ok = bongo_cat_items_remove_plugin(app, &error); notice(app, ok, &error);
    }
    bongo_cat_pref_card_end(context, &saved);
}
