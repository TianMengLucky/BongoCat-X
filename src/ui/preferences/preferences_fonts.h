#ifndef BONGO_CAT_PREFERENCES_FONTS_H
#define BONGO_CAT_PREFERENCES_FONTS_H

#include "preferences_state.h"

typedef struct BongoCatPreferenceFonts {
    char body_path[BONGO_CAT_PATH_CAP];
    char body_fallback_path[BONGO_CAT_PATH_CAP];
    char body_korean_fallback_path[BONGO_CAT_PATH_CAP];
    char heading_path[BONGO_CAT_PATH_CAP];
    char heading_fallback_path[BONGO_CAT_PATH_CAP];
    char heading_korean_fallback_path[BONGO_CAT_PATH_CAP];
    const char *body;
    const char *body_fallback;
    const char *body_korean_fallback;
    const char *heading;
    const char *heading_fallback;
    const char *heading_korean_fallback;
    const nk_rune *ranges;
    const nk_rune *heading_ranges;
} BongoCatPreferenceFonts;

void bongo_cat_preferences_fonts_resolve(BongoCatPreferences *preferences,
    BongoCatPreferenceFonts *fonts);

#endif
