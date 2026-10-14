#include "preferences_page_dispatch.h"
#include "bongo_cat/statistics.h"
#include "bongo_cat/items.h"
#include "bongo_cat/sounds.h"
#include "bongo_cat/store.h"
#include "preferences_theme.h"
#include "preferences_widgets.h"

bool bongo_cat_preferences_subnavigation(BongoCatPreferences *value, struct nk_context *context) {
    bool preferences = value->page == 0 || value->page == 2;
    bool models = value->page == 1 || value->page == 8;
    if (!preferences && !models) return false;
    const char *labels[] = {
        bongo_cat_i18n_get(value->app->i18n, preferences ? "pages.preference.navigation.settings" :
            "pages.preference.navigation.localModels", preferences ? "Settings" : "Local models"),
        bongo_cat_i18n_get(value->app->i18n, preferences ? "pages.preference.shortcut.title" :
            "pages.preference.store.title", preferences ? "Shortcuts" : "Store")};
    nk_layout_row_dynamic(context, 42, 1);
    int current = value->page == 2 || value->page == 8;
    int selected = bongo_cat_pref_page_tabs(context, preferences ? "preferences-sections" : "model-sections",
        labels, 2, current);
    if (selected != current) {
        if (value->shortcut_recording) bongo_cat_preferences_shortcut_cancel(value);
        value->page = preferences ? (selected ? 2 : 0) : (selected ? 8 : 1);
        value->render_dirty = true;
    }
    return true;
}

void bongo_cat_preferences_draw_page(BongoCatPreferences *value,
    struct nk_context *context) {
    if (value->page == 4 && !bongo_cat_statistics_page(value->app)) value->page = 3;
    if (value->page == 6 && !bongo_cat_items_page(value->app)) value->page = 3;
    if (value->page == 7 && !bongo_cat_sounds_page(value->app)) value->page = 3;
    switch (value->page) {
    case 0: bongo_cat_preferences_page_settings(value, context); break;
    case 1: bongo_cat_preferences_page_model(value, context); break;
    case 2: bongo_cat_preferences_page_shortcuts(value, context); break;
    case 3: bongo_cat_preferences_page_plugins(value, context); break;
    case 4: bongo_cat_preferences_page_statistics(value, context); break;
    case 8:
        if (bongo_cat_store_active(value->app)) bongo_cat_preferences_page_store(value, context);
        else {
            bongo_cat_pref_intro(context, bongo_cat_i18n_get(value->app->i18n,
                "pages.preference.navigation.storeUnavailable", "Enable the Store plugin on the Plugins page to browse community models."));
            nk_layout_row_static(context, 36, 160, 1);
            if (nk_button_label(context, bongo_cat_i18n_get(value->app->i18n,
                "pages.preference.plugins.title", "Plugins"))) {
                value->page = 3; value->render_dirty = true;
            }
        }
        break;
    case 7: bongo_cat_preferences_page_sounds(value, context); break;
    case 6: bongo_cat_preferences_page_items(value, context); break;
    default: bongo_cat_preferences_page_about(value, context); break;
    }
}
