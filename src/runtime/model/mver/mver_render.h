#ifndef BONGO_CAT_MVER_RENDER_H
#define BONGO_CAT_MVER_RENDER_H
#include "bongo_cat/model.h"
#include "bongo_cat/json_dom.h"
void bongo_cat_mver_render_options(BongoJsonValue *config,
    BongoCatModelRuntimeRenderOptions *options);
bool bongo_cat_mver_render_read(const char *path,
    BongoCatModelRuntimeRenderOptions *options);
#endif
