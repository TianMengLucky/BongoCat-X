#ifndef BONGO_CAT_PLUGIN_PAGE_H
#define BONGO_CAT_PLUGIN_PAGE_H
#include "bongo_cat/plugin_ui_page.h"
#include <SDL3/SDL.h>
typedef struct BongoCatPluginPageSession {
    const BongoCatPluginPageUiContribution *api;
    void *instance;
    bool queried;
} BongoCatPluginPageSession;
const BongoCatPluginPageUiContribution *bongo_cat_plugin_page_query(
    BongoCatPluginPageSession *session, SDL_SharedObject *library,
    const char *symbol, void **instance);
void bongo_cat_plugin_page_stop(BongoCatPluginPageSession *session);
#endif
