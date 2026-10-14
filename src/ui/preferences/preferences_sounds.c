#include "preferences_state.h"
#include "preferences_widgets.h"
#include "preferences_controls.h"
#include "preferences_notice.h"
#include "preferences_model_glyphs.h"
#include "bongo_cat/sounds.h"
#include "bongo_cat/i18n.h"
#include "bongo_cat/preferences.h"
#include "ui_paint.h"
#include <stdio.h>
#include <string.h>
static const char *label(BongoCatApp *app, const char *key, const char *fallback) {
    char full[128]; snprintf(full, sizeof(full), "pages.preference.sounds.%s", key);
    return bongo_cat_i18n_get(app->i18n, full, fallback);
}
static void failure(BongoCatApp *app) {
    bongo_cat_preferences_notice_show(app, label(app, "failed", "Unable to update sound effects"), true);
}
void bongo_cat_preferences_sounds_import(BongoCatApp *app, const char *key, const char *path) {
    BongoCatError error = {0};
    if (!bongo_cat_sounds_import_audio(app, key, path, &error)) failure(app);
    else bongo_cat_preferences_notice_show(app, label(app, "queued", "Audio import queued"), false);
}
static void keyboard(BongoCatPreferences *value, struct nk_context *context, const BongoCatSoundScene *scene) {
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    const char *hover = NULL;
    bool picker = bongo_cat_preferences_sound_picker_open(value);
    for (unsigned row = 0; row < 6; ++row) {
        nk_layout_row_dynamic(context, 42, 1); struct nk_rect area;
        if (nk_widget(&area, context) == NK_WIDGET_INVALID) continue;
        float unit = area.w / 23;
        struct nk_command_buffer *canvas = nk_window_get_canvas(context);
        for (unsigned i = 0; i < scene->count; ++i) {
            BongoCatSoundKey key;
            if (!bongo_cat_sounds_key(value->app, i, &key)) continue;
            for (unsigned copy = 0; copy < 2; ++copy) {
                if (copy) {
                    if (!strcmp(key.id, "Return")) { key.x = 22; key.width = 1; key.row = 4; }
                    else if (!strcmp(key.id, "Meta")) { key.x = 11.75f; key.width = 1.25f; }
                    else continue;
                }
                if (key.row != row) continue;
                struct nk_rect rect = nk_rect(area.x + key.x * unit + 1, area.y + 1, key.width * unit - 2, 38);
                bool over = nk_input_is_mouse_hovering_rect(&context->input, rect);
                bool bound = scene->bindings[i].asset[0] != 0;
                nk_fill_rect(canvas, rect, 5, over ? p.selection : bound ? p.selection : p.field);
                nk_stroke_rect(canvas, rect, 5, value->selected_sound == (int)i ? 2.0f : 1.0f,
                    value->selected_sound == (int)i ? p.accent : p.border_subtle);
                const struct nk_user_font *font = value->ui.caption_font ? value->ui.caption_font : context->style.font;
                float text_width = font->width(font->userdata, font->height, key.label, nk_strlen(key.label));
                nk_draw_text(canvas, nk_rect(rect.x + NK_MAX(1.0f, (rect.w - text_width) / 2),
                    rect.y + (rect.h - font->height) / 2, NK_MAX(1.0f, rect.w - 2), font->height),
                    key.label, nk_strlen(key.label), font, nk_rgba(0, 0, 0, 0), bound ? p.accent : p.text);
                if (bound) nk_fill_circle(canvas, nk_rect(rect.x + rect.w - 5, rect.y + rect.h - 5, 3, 3), p.accent);
                if (over) {
                    hover = bound ? scene->bindings[i].name : label(value->app, "unassigned", "No sound assigned");
                    if (!picker && nk_input_is_mouse_click_in_rect(&context->input, NK_BUTTON_LEFT, rect)) {
                        value->selected_sound = (int)i;
                        BongoCatError error = {0};
                        if (bound && !bongo_cat_sounds_preview(value->app, i, &error)) failure(value->app);
                        (void)bongo_cat_preferences_import_sound_open(value->import_dialog, value->window, key.id);
                        value->render_dirty = true;
                    }
                }
            }
        }
    }
    if (hover) nk_tooltip(context, hover);
}
void bongo_cat_preferences_page_sounds(BongoCatPreferences *value, struct nk_context *context) {
    BongoCatApp *app = value->app; BongoCatSoundScene scene;
    if (!bongo_cat_sounds_snapshot(app, &scene)) {
        nk_layout_row_dynamic(context, 45, 1);
        nk_label_wrap(context, bongo_cat_sounds_busy(app) ? label(app, "busy", "Worker is processing audio") :
            label(app, "failed", "Unable to update sound effects")); return;
    }
    bongo_cat_pref_intro(context, label(app, "help", "Click a key to preview its sound and choose an audio file. Hover to see the filename. Physical key presses play the assigned sound; bindings save automatically."));
    uint64_t hash = UINT64_C(14695981039346656037); bool missing = false;
    for (unsigned i = 0; i < scene.count; ++i) if (scene.bindings[i].name[0]) {
        if (!bongo_cat_preferences_model_glyphs_ready(value, scene.bindings[i].name)) missing = true;
        for (const unsigned char *p = (const unsigned char *)scene.bindings[i].name; *p; ++p)
            hash = (hash ^ *p) * UINT64_C(1099511628211);
    }
    if (missing && hash != value->sound_glyph_hash) { value->sound_glyph_hash = hash; value->font_reload_pending = true; }
    keyboard(value, context, &scene);
    value->selected_sound = NK_CLAMP(0, value->selected_sound, (int)scene.count - 1);
    BongoCatSoundKey key;
    if (!bongo_cat_sounds_key(app, (unsigned)value->selected_sound, &key)) return;
    nk_layout_row_dynamic(context, 30, 2);
    nk_label(context, key.id, NK_TEXT_LEFT);
    nk_label(context, scene.bindings[value->selected_sound].name[0] ? scene.bindings[value->selected_sound].name :
        label(app, "unassigned", "No sound assigned"), NK_TEXT_LEFT);
    nk_layout_row_dynamic(context, 32, 3);
    if (nk_button_label(context, label(app, "choose", "Choose audio")))
        (void)bongo_cat_preferences_import_sound_open(value->import_dialog, value->window, key.id);
    if (nk_button_label(context, label(app, "preview", "Preview"))) {
        BongoCatError error = {0}; if (!bongo_cat_sounds_preview(app, (unsigned)value->selected_sound, &error)) failure(app);
    }
    if (nk_button_label(context, label(app, "clear", "Clear binding"))) {
        BongoCatError error = {0}; if (!bongo_cat_sounds_clear(app, (unsigned)value->selected_sound, &error)) failure(app);
    }
    nk_layout_row_dynamic(context, 58, 1);
    nk_label_wrap(context, label(app, "formats", "WAV, MP3, Ogg Vorbis and FLAC, up to 64 MB. Sound runs independently of the selected model. Key availability depends on global input permission and keyboard layout."));
}
void bongo_cat_preferences_sounds_plugin_card(BongoCatPreferences *value, struct nk_context *context) {
    BongoCatApp *app = value->app; BongoCatSoundsInfo info; bongo_cat_sounds_info(app, &info);
    struct nk_style_window saved;
    if (!bongo_cat_pref_card_begin(context, "sounds-plugin", &saved)) return;
    nk_layout_row_dynamic(context, 30, 1); nk_label(context, label(app, "title", "Sound effects"), NK_TEXT_LEFT);
    nk_layout_row_dynamic(context, 76, 1);
    nk_label_wrap(context, label(app, "pluginHint", "Assign a sound to each keyboard key. A dedicated worker handles import, decoding, playback and saving. Disabling preserves bindings and audio files."));
    nk_layout_row_dynamic(context, 25, 1);
    nk_label(context, bongo_cat_i18n_get(app->i18n, info.active ? "pages.preference.plugins.active" :
        !info.installed ? "pages.preference.plugins.missing" : "pages.preference.plugins.disabled",
        info.active ? "In use" : !info.installed ? "Not installed" : "Disabled"), NK_TEXT_LEFT);
    nk_layout_row_dynamic(context, 30, 2);
    nk_label(context, bongo_cat_i18n_get(app->i18n, "pages.preference.plugins.enabled", "Enabled"), NK_TEXT_LEFT);
    bool enabled = info.enabled; bool busy = bongo_cat_sounds_busy(app);
    if (bongo_cat_pref_control_toggle_available(context, "enable-sounds-plugin", &enabled, info.installed && !busy)) {
        BongoCatError error = {0};
        if (!bongo_cat_sounds_set_enabled(app, enabled, &error)) failure(app);
        bongo_cat_preferences_invalidate(app->preferences);
    }
    nk_layout_row_dynamic(context, 30, 1);
    if (busy) nk_label(context, label(app, "busy", "Worker is processing audio"), NK_TEXT_LEFT);
    else if (info.managed && nk_button_label(context, bongo_cat_i18n_get(app->i18n, "pages.preference.plugins.remove", "Remove plugin"))) {
        BongoCatError error = {0}; if (!bongo_cat_sounds_remove_plugin(app, &error)) failure(app);
    }
    bongo_cat_pref_card_end(context, &saved);
}
