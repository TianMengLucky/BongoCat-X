#ifndef BONGO_CAT_MODEL_GEOMETRY_H
#define BONGO_CAT_MODEL_GEOMETRY_H

#include "runtime.h"

typedef struct BongoCatModelContentAnchor {
    int x, y;
    bool valid;
} BongoCatModelContentAnchor;

BongoCatModelContentAnchor bongo_cat_model_content_anchor(BongoCatApp *app);
bool bongo_cat_model_texture_display_size(void *userdata,
    const BongoCatModelRuntimeRenderOptions *options, int canvas_width,
    int canvas_height, int *display_width, int *display_height);
bool bongo_cat_model_apply_aspect(BongoCatApp *app,
    const BongoCatModelRuntimeRenderOptions *options,
    const BongoCatModelContentAnchor *anchor, bool replacing_model);

#endif
