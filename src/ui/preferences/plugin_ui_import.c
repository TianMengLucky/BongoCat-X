#include "preferences_state.h"
#include "preferences_notice.h"
#include "bongo_cat/items.h"
#include "bongo_cat/sounds.h"
#include "bongo_cat/preferences.h"

void bongo_cat_preferences_sounds_import(BongoCatApp *app, const char *key, const char *path) {
    BongoCatError error = {0};
    bool ok = bongo_cat_sounds_import_audio(app, key, path, &error);
    bongo_cat_preferences_notice_show(app, bongo_cat_i18n_get(app->i18n,
        ok ? "pages.preference.sounds.queued" : "pages.preference.sounds.failed",
        ok ? "Audio import queued" : "Unable to update sound effects"), !ok);
}
void bongo_cat_preferences_items_import(BongoCatApp *app, const char *path) {
    if (!bongo_cat_items_queue_import(app, path))
        bongo_cat_preferences_notice_show(app, bongo_cat_i18n_get(app->i18n,
            "pages.preference.items.failed", "Unable to update items"), true);
}
void bongo_cat_preferences_statistics_invalidate(BongoCatPreferences *value) {
    if (value && value->visible && value->page == 4) value->render_dirty = true;
}

void bongo_cat_preferences_plugin_fonts_changed(BongoCatPreferences *value) {
    if (value) { value->font_reload_pending = true; value->render_dirty = true; }
}
