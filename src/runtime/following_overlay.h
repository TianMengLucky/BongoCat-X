#ifndef BONGO_CAT_FOLLOWING_OVERLAY_H
#define BONGO_CAT_FOLLOWING_OVERLAY_H
#include "bongo_cat/app.h"
#include <SDL3/SDL_events.h>
#include "bongo_cat/plugin_overlay.h"
typedef struct BongoCatFollowingOverlay BongoCatFollowingOverlay;
void bongo_cat_following_overlay_destroy(BongoCatFollowingOverlay **);
bool bongo_cat_following_overlay_event(BongoCatFollowingOverlay *, const SDL_Event *);
void bongo_cat_following_overlay_update(BongoCatApp *, BongoCatFollowingOverlay **,
    const BongoCatPluginOverlay *, void *, bool enabled, bool left, uint64_t *retry);
#endif
