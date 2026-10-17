#include "plugin_page.h"

const BongoCatPluginPageUiContribution *bongo_cat_plugin_page_query(
    BongoCatPluginPageSession *session, SDL_SharedObject *library,
    const char *symbol, void **instance) {
    if (!session || !library || !instance) return NULL;
    *instance = NULL;
    if (!session->queried) {
        session->queried = true;
        union { SDL_FunctionPointer function; BongoCatPluginPageUiQuery query; } entry;
        entry.function = SDL_LoadFunction(library, symbol);
        if (!entry.function) SDL_ClearError();
        const BongoCatPluginPageUiContribution *api = entry.function ?
            entry.query(BONGO_CAT_PLUGIN_PAGE_UI_ABI) : NULL;
        if (api && api->abi_version == BONGO_CAT_PLUGIN_PAGE_UI_ABI &&
            api->struct_size == sizeof(*api) && api->create && api->destroy && api->draw) {
            session->instance = api->create();
            if (session->instance) session->api = api;
        }
    }
    *instance = session->instance;
    return session->api;
}

void bongo_cat_plugin_page_stop(BongoCatPluginPageSession *session) {
    if (!session) return;
    if (session->api && session->instance) session->api->destroy(session->instance);
    *session = (BongoCatPluginPageSession){0};
}
