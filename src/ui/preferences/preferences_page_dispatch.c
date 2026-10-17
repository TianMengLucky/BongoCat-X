#include "preferences_page_dispatch.h"
#include "plugin_ui_internal.h"
#include "bongo_cat/statistics.h"
#include "bongo_cat/items.h"
#include "bongo_cat/sounds.h"
#include "bongo_cat/store.h"
#include "preferences_theme.h"
#include "preferences_widgets.h"

bool bongo_cat_preferences_subnavigation(BongoCatPreferences *value, struct nk_context *context) {
    bool preferences = value->page == 0 || value->page == 2;
    bool models = value->page == 1 || value->page == 8;
    bool statistics = value->page == 4 || value->page == 9;
    if (statistics) {
        const char *labels[] = {
            bongo_cat_i18n_get(value->app->i18n,
                "pages.preference.statistics.input", "Keyboard and mouse"),
            bongo_cat_i18n_get(value->app->i18n,
                "pages.preference.statistics.applications", "Application usage")};
        nk_layout_row_dynamic(context, 42, 1);
        int current = value->page == 9;
        int selected = bongo_cat_pref_page_tabs(context, "statistics-sections", labels, 2, current);
        if (selected != current) {
            value->page = selected ? 9 : 4;
            value->render_dirty = true;
        }
        return true;
    }
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

static void plugin_page(BongoCatPreferences *value, struct nk_context *context,
    enum BongoCatPluginUiKind kind) {
    if (!bongo_cat_preferences_plugin_page(value, context, kind))
        bongo_cat_pref_intro(context, bongo_cat_i18n_get(value->app->i18n,
            "pages.preference.plugins.pageUiUnavailable",
            "Update this plugin to display its settings page."));
}

void bongo_cat_preferences_draw_page(BongoCatPreferences *value,
    struct nk_context *context) {
    if ((value->page == 4 || value->page == 9) && !bongo_cat_statistics_page(value->app)) value->page = 3;
    if (value->page == 6 && !bongo_cat_items_page(value->app)) value->page = 3;
    if (value->page == 7 && !bongo_cat_sounds_page(value->app)) value->page = 3;
    switch (value->page) {
    case 0: bongo_cat_preferences_page_settings(value, context); break;
    case 1: bongo_cat_preferences_page_model(value, context); break;
    case 2: bongo_cat_preferences_page_shortcuts(value, context); break;
    case 3: bongo_cat_preferences_page_plugins(value, context); break;
    case 9:
    case 4: plugin_page(value, context, BONGO_CAT_PLUGIN_UI_STATISTICS); break;
    case 8:
        if (bongo_cat_store_active(value->app)) plugin_page(value, context, BONGO_CAT_PLUGIN_UI_STORE);
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
    case 7: plugin_page(value, context, BONGO_CAT_PLUGIN_UI_SOUNDS); break;
    case 6: plugin_page(value, context, BONGO_CAT_PLUGIN_UI_ITEMS); break;
    default: bongo_cat_preferences_page_about(value, context); break;
    }
}
