#ifndef BONGO_CAT_PREFERENCES_OPTIONS_H
#define BONGO_CAT_PREFERENCES_OPTIONS_H

#include "preferences_widgets.h"
#include "bongo_cat/preferences.h"

enum BongoCatOptionsSection {
    BONGO_CAT_OPTIONS_WINDOW,
    BONGO_CAT_OPTIONS_MODEL,
    BONGO_CAT_OPTIONS_APPLICATION,
    BONGO_CAT_OPTIONS_APPEARANCE,
    BONGO_CAT_OPTIONS_PERMISSIONS
};

bool bongo_cat_preferences_options_begin(BongoCatPreferences *value,
    struct nk_context *context, enum BongoCatOptionsSection section,
    const char *title, BongoCatPrefIcon icon);
void bongo_cat_preferences_options_end(BongoCatPreferences *value, struct nk_context *context);

#endif
