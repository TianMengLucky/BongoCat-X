#ifndef BONGO_CAT_PREFERENCES_NOTICE_H
#define BONGO_CAT_PREFERENCES_NOTICE_H

#include "bongo_cat/app.h"
#include "bongo_cat/preferences.h"
#include "nuklear_config.h"

void bongo_cat_preferences_notice_show(BongoCatApp *app,
    const char *message, bool error);
/* Same toast with an explicit duration and corner anchor (bottom-right). */
void bongo_cat_preferences_notice_show_anchored(BongoCatApp *app,
    const char *message, bool error, unsigned duration_ms,
    bool bottom_right);
/* Dismiss every active toast immediately (used when a state change makes
   them obsolete, e.g. the missing-Core notice after the Core is loaded). */
void bongo_cat_preferences_notice_clear(BongoCatApp *app);
/* Persistent update toast, retained while Settings is closed. */
void bongo_cat_preferences_notice_show_update(BongoCatApp *app, const char *message);
/* Consume toast clicks before they reach controls or window dragging. */
bool bongo_cat_preferences_notice_event(BongoCatPreferences *value, const SDL_Event *event);
void bongo_cat_preferences_notice_draw(BongoCatPreferences *preferences,
    struct nk_context *context, float width, float height);

#endif
