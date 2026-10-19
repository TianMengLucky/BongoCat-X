#ifndef BONGO_CAT_LAN_H
#define BONGO_CAT_LAN_H
#include "bongo_cat/app.h"
#include <SDL3/SDL_events.h>
#include "bongo_cat/lan_plugin.h"
void bongo_cat_lan_start(BongoCatApp *);
void bongo_cat_lan_stop(BongoCatApp *);
void bongo_cat_lan_update(BongoCatApp *);
void bongo_cat_lan_release_graphics(BongoCatApp *);
bool bongo_cat_lan_event(BongoCatApp *, const SDL_Event *);
uint32_t bongo_cat_lan_settings(BongoCatApp *, BongoCatPluginUi *,
    const BongoCatPluginPageUiHost *, const BongoCatPluginPageUiServices *);
bool bongo_cat_lan_import(BongoCatApp *, const char *, bool *, BongoCatError *);
#endif
