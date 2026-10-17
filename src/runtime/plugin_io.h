#ifndef BONGO_CAT_RUNTIME_PLUGIN_IO_H
#define BONGO_CAT_RUNTIME_PLUGIN_IO_H
#include "bongo_cat/plugin_io.h"
#include "bongo_cat/audio.h"
#include <SDL3/SDL.h>
typedef struct BongoCatPluginIoContext {
    SDL_Mutex *mutex;
    BongoCatAudio *audio;
} BongoCatPluginIoContext;
const BongoCatPluginIo *bongo_cat_plugin_io_query(SDL_SharedObject *library, const char *symbol);
BongoCatPluginIoHost bongo_cat_plugin_io_host(BongoCatPluginIoContext *context);
#endif
