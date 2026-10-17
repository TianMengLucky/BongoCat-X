#ifndef BONGO_CAT_RUNTIME_PLUGIN_EVENTS_H
#define BONGO_CAT_RUNTIME_PLUGIN_EVENTS_H
#include "bongo_cat/plugin_events.h"
#include <SDL3/SDL.h>
typedef struct BongoCatApp BongoCatApp;
const BongoCatPluginEvents *bongo_cat_plugin_events_query(SDL_SharedObject *, const char *);
uint32_t bongo_cat_plugin_event(BongoCatApp *, const BongoCatPluginEvents *, void *,
    uint32_t event, uint32_t result);
#endif
