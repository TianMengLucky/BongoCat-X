#include "plugin_events.h"
#include "bongo_cat/app.h"
#include "bongo_cat/i18n.h"
#include "bongo_cat/preferences.h"
#include "preferences_notice.h"
const BongoCatPluginEvents *bongo_cat_plugin_events_query(SDL_SharedObject *library, const char *symbol) {
    union { SDL_FunctionPointer function; BongoCatPluginEventsQuery query; } entry;
    entry.function = SDL_LoadFunction(library, symbol);
    const BongoCatPluginEvents *api = entry.function ? entry.query(BONGO_CAT_PLUGIN_EVENTS_ABI) : NULL;
    if (!entry.function) SDL_ClearError();
    return api && api->abi_version == BONGO_CAT_PLUGIN_EVENTS_ABI &&
        api->struct_size == sizeof(*api) && api->event ? api : NULL;
}
uint32_t bongo_cat_plugin_event(BongoCatApp *app, const BongoCatPluginEvents *api,
    void *instance, uint32_t event, uint32_t result) {
    if (!app || !api) return 0;
    BongoCatPluginNotification notification = {0};
    if (!api->event(instance, event, result, &notification)) return 0;
    if (notification.key && notification.fallback)
        bongo_cat_preferences_notice_show(app, bongo_cat_i18n_get(app->i18n,
            notification.key, notification.fallback), notification.error != 0);
    if (notification.flags & BONGO_PLUGIN_EVENT_INVALIDATE)
        bongo_cat_preferences_invalidate(app->preferences);
    return notification.flags;
}
