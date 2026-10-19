#ifndef BONGO_CAT_COMPANION_H
#define BONGO_CAT_COMPANION_H
#include "bongo_cat/app.h"
#include <SDL3/SDL_events.h>
#include "bongo_cat/companion_plugin.h"
void bongo_cat_companion_start(BongoCatApp *);
void bongo_cat_companion_stop(BongoCatApp *);
void bongo_cat_companion_event(BongoCatApp *, uint32_t);
void bongo_cat_companion_update(BongoCatApp *);
bool bongo_cat_companion_overlay_event(BongoCatApp *, const SDL_Event *);
bool bongo_cat_companion_active(BongoCatApp *);
uint32_t bongo_cat_companion_settings(BongoCatApp *, BongoCatPluginUi *,
    const BongoCatPluginPageUiHost *, const BongoCatPluginPageUiServices *);
bool bongo_cat_companion_import(BongoCatApp *, const char *, bool *, BongoCatError *);
/* Optional TTS provider. UTF-8 text is borrowed for speak only; copy it before
   queuing synthesis. Callbacks run on the main thread and must not block.
   cancel must stop queued speech before unregister/unload. No provider by default. */
typedef struct BongoCatCompanionTts {
    uint32_t abi_version, struct_size;
    void *context;
    void (*speak)(void *, const char *);
    void (*cancel)(void *);
} BongoCatCompanionTts;
bool bongo_cat_companion_set_tts(BongoCatApp *, const BongoCatCompanionTts *);
#endif
