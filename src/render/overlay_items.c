#include "overlay_internal.h"
#include <string.h>
void bongo_cat_overlay_set_items(BongoCatOverlay *value, BongoCatOverlayItemsDraw draw,
    void *userdata, uint64_t revision) {
    if (!value || (value->items_draw == draw && value->items_userdata == userdata &&
        value->items_revision == revision)) return;
    value->items_draw = draw; value->items_userdata = userdata; value->items_revision = revision;
    value->native_canvas_uploaded = false;
    SDL_DestroySurface(value->native_canvas); value->native_canvas = NULL;
    memset(value->native_geometry, 0, sizeof(value->native_geometry));
    ++value->native_input_revision;
}
