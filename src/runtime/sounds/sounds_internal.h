#ifndef BONGO_CAT_SOUNDS_INTERNAL_H
#define BONGO_CAT_SOUNDS_INTERNAL_H
#include "plugin_page.h"
#include "plugin_events.h"
#include "bongo_cat/app.h"
#include "bongo_cat/sounds.h"
#include "bongo_cat/audio.h"
#include <SDL3/SDL.h>
#include "plugin_worker.h"
#include "plugin_io.h"
typedef struct BongoCatSounds {
    SDL_SharedObject *library;
    const BongoCatPluginEvents *events;
    BongoCatPluginPageSession page_ui;
    const BongoCatSoundsPlugin *api;
    const BongoCatPluginIo *io;
    const BongoCatPluginUiContribution *ui;
    void *instance;
    BongoCatSoundsPage page;
    BongoCatAudio *audio;
    BongoCatPluginWorker worker;
    atomic_bool loaded;
} BongoCatSounds;
bool bongo_cat_sounds_managed_path(BongoCatApp *app, const char *suffix, char *out);
const BongoCatSoundsPlugin *bongo_cat_sounds_query(SDL_SharedObject *library);
bool bongo_cat_sounds_load_scene(BongoCatApp *app, BongoCatError *error);
bool bongo_cat_sounds_save(BongoCatApp *app, BongoCatError *error);
bool bongo_cat_sounds_worker_start(BongoCatApp *app);
bool bongo_cat_sounds_import_audio_now(BongoCatApp *app, const char *key, const char *path, BongoCatError *error);
bool bongo_cat_sounds_clear_now(BongoCatApp *app, uint32_t index, BongoCatError *error);
bool bongo_cat_sounds_preview_now(BongoCatApp *app, uint32_t index, BongoCatError *error);
void bongo_cat_sounds_input_now(BongoCatApp *app, const BongoCatInputEvent *event, bool play);
bool bongo_cat_sounds_execute(BongoCatApp *app, uint32_t operation, uint32_t index,
    const char *key, const char *path, bool play);
#endif
