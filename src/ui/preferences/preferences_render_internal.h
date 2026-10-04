#ifndef BONGO_CAT_PREFERENCES_RENDER_INTERNAL_H
#define BONGO_CAT_PREFERENCES_RENDER_INTERNAL_H

#include "bongo_cat/preferences.h"

bool bongo_cat_preferences_draw_frame(BongoCatPreferences *value,
    float width, float height, bool dark);

/* Drives the preferences window opacity fade; returns true when the caller
   must skip painting this frame (fade in progress or close just finished). */
bool bongo_cat_preferences_window_fade_tick(BongoCatPreferences *value);

#endif
