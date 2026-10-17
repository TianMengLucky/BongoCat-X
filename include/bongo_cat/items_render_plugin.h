#ifndef BONGO_CAT_ITEMS_RENDER_PLUGIN_H
#define BONGO_CAT_ITEMS_RENDER_PLUGIN_H
#include "bongo_cat/items_plugin.h"
/* Renderer-neutral scene policy. Host backends execute placements as generic
   image operations; visibility, layer order and model-relative transforms are
   decided by the plugin. No graphics handle or SDL type crosses the ABI. */
typedef struct BongoCatItemViewport {
    int32_t width, height, x, y, content_width, content_height;
    uint32_t mirror, vertical_flip;
} BongoCatItemViewport;
typedef struct BongoCatItemPlacement {
    float x, y, width, height;
    double angle;
    uint32_t flip;
} BongoCatItemPlacement;
typedef struct BongoCatItemsRenderPlugin {
    uint32_t abi_version, struct_size;
    uint32_t (*order)(const BongoCatItemScene *, uint32_t front, uint32_t *, uint32_t);
    uint32_t (*placement)(const BongoCatItem *, const BongoCatItemViewport *,
        int32_t image_width, int32_t image_height, BongoCatItemPlacement *);
} BongoCatItemsRenderPlugin;
typedef const BongoCatItemsRenderPlugin *(*BongoCatItemsRenderQuery)(uint32_t);
#endif
