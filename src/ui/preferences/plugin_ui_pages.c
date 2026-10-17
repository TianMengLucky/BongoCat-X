#include "plugin_ui_internal.h"
#include "preferences_widgets.h"
#include "bongo_cat/items.h"
#include "bongo_cat/sounds.h"
#include "bongo_cat/statistics.h"
#include "bongo_cat/store.h"

bool bongo_cat_preferences_plugin_page(BongoCatPreferences *value,
    struct nk_context *context, enum BongoCatPluginUiKind kind) {
    if (!value || !context) return false;
    void *instance = NULL;
    const BongoCatPluginPageUiContribution *api = NULL;
    switch (kind) {
    case BONGO_CAT_PLUGIN_UI_SOUNDS: api = bongo_cat_sounds_page_ui(value->app, &instance); break;
    case BONGO_CAT_PLUGIN_UI_ITEMS: api = bongo_cat_items_page_ui(value->app, &instance); break;
    case BONGO_CAT_PLUGIN_UI_STATISTICS: api = bongo_cat_statistics_page_ui(value->app, &instance); break;
    case BONGO_CAT_PLUGIN_UI_STORE: api = bongo_cat_store_page_ui(value->app, &instance); break;
    default: return false;
    }
    if (!api || !instance) return false;
    BongoCatPluginPageBinding binding = {value, kind};
    BongoCatPluginPageUiServices services = {
        BONGO_CAT_PLUGIN_PAGE_UI_ABI, sizeof(BongoCatPluginPageUiServices), &binding,
        bongo_cat_preferences_plugin_read, bongo_cat_preferences_plugin_action
    };
    BongoCatPluginUi ui = {0};
    ui.context = context; ui.i18n = value->app->i18n; ui.preferences = value;
    bool drawn = api->draw(instance, &ui, bongo_cat_preferences_plugin_canvas(), &services) != 0;
    /* Restore a partially drawn card even if a plugin catches a panic. */
    if (ui.card_open) bongo_cat_pref_card_end(context, &ui.saved);
    return drawn;
}
