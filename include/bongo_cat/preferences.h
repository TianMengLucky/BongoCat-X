#ifndef BONGO_CAT_PREFERENCES_H
#define BONGO_CAT_PREFERENCES_H

#include "bongo_cat/common.h"

typedef union SDL_Event SDL_Event;
typedef struct SDL_Window SDL_Window;
typedef struct BongoCatApp BongoCatApp;
typedef struct BongoCatPreferences BongoCatPreferences;

BongoCatPreferences *bongo_cat_preferences_create(BongoCatApp *app);
void bongo_cat_preferences_destroy(BongoCatPreferences *preferences);
void bongo_cat_preferences_show(BongoCatPreferences *preferences);
void bongo_cat_preferences_close(BongoCatPreferences *preferences);
bool bongo_cat_preferences_visible(const BongoCatPreferences *preferences);
bool bongo_cat_preferences_needs_frame(BongoCatPreferences *preferences);
void bongo_cat_preferences_model_watch(BongoCatPreferences *preferences,
    uint64_t now);
void bongo_cat_preferences_input_begin(BongoCatPreferences *preferences);
void bongo_cat_preferences_input_end(BongoCatPreferences *preferences);
bool bongo_cat_preferences_event(BongoCatPreferences *preferences, const SDL_Event *event);
bool bongo_cat_preferences_shortcuts_blocked(
    const BongoCatPreferences *preferences);
void bongo_cat_preferences_render(BongoCatPreferences *preferences);
void bongo_cat_preferences_invalidate(BongoCatPreferences *preferences);
void bongo_cat_preferences_process_model_selection(
    BongoCatPreferences *preferences);
void bongo_cat_preferences_model_load_progress(
    BongoCatPreferences *preferences, float progress);
void bongo_cat_preferences_models_changed(BongoCatPreferences *preferences);
void bongo_cat_preferences_request_model_import(BongoCatPreferences *preferences);
bool bongo_cat_preferences_open_model_import(BongoCatPreferences *preferences,
    SDL_Window *parent);
/* Opens the file picker for the runtime Live2D Core import. */
void bongo_cat_preferences_request_sdk_import(BongoCatPreferences *preferences);
/* Re-runs the drop-in folder scan for the Live2D Core on a worker thread;
   the completion (including the GL hot swap) lands on the main thread. */
void bongo_cat_preferences_request_live2d_rescan(BongoCatPreferences *preferences);

#endif
