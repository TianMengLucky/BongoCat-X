#ifndef BONGO_CAT_PREFERENCES_LIVE2D_SETUP_H
#define BONGO_CAT_PREFERENCES_LIVE2D_SETUP_H
#include "bongo_cat/preferences.h"
#include "preferences_scrollbar.h"
#include <SDL3/SDL.h>
#include <stdatomic.h>
enum { BONGO_CAT_SDK_LICENSE_LOADING = 1, BONGO_CAT_SDK_LICENSE_READY,
    BONGO_CAT_SDK_DOWNLOADING, BONGO_CAT_SDK_COMPLETE, BONGO_CAT_SDK_FAILED };
typedef struct BongoCatLive2DSetup {
    SDL_Thread *thread;
    atomic_int phase;
    atomic_bool cancelled;
    atomic_uint_fast64_t received, total;
    unsigned event_type, observed_phase;
    bool open, input_armed, licenses_ready;
    uint64_t opened_ns;
    float scroll, text_height, text_width;
    BongoCatPreferencesScrollbar scrollbar;
    char language[3], target[BONGO_CAT_PATH_CAP], message[512];
    char terms[196608];
} BongoCatLive2DSetup;
void bongo_cat_preferences_live2d_setup_request(BongoCatPreferences *value);
void bongo_cat_preferences_live2d_setup_poll(BongoCatPreferences *value);
void bongo_cat_preferences_live2d_setup_stop(BongoCatPreferences *value);
void bongo_cat_preferences_live2d_setup_cancel(BongoCatPreferences *value);
void bongo_cat_preferences_live2d_setup_accept(BongoCatPreferences *value);
bool bongo_cat_preferences_live2d_setup_active(const BongoCatPreferences *value);
const char *bongo_cat_preferences_live2d_setup_terms(const BongoCatPreferences *value);
void bongo_cat_preferences_live2d_setup_draw(BongoCatPreferences *value, struct nk_context *context);
#endif
