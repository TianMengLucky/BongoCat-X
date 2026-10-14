#ifndef BONGO_CAT_PREFERENCES_PAGE_DISPATCH_H
#define BONGO_CAT_PREFERENCES_PAGE_DISPATCH_H

#include "preferences_state.h"

bool bongo_cat_preferences_subnavigation(BongoCatPreferences *value, struct nk_context *context);
void bongo_cat_preferences_draw_page(BongoCatPreferences *value,
    struct nk_context *context);

#endif
