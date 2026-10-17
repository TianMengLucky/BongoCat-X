#ifndef BONGO_CAT_PLUGIN_OVERLAY_H
#define BONGO_CAT_PLUGIN_OVERLAY_H
#include "bongo_cat/plugin_ui_page.h"
#include <stddef.h>
/* Reuses page rectangles, font roles and palette roles. The host owns windows,
   fonts and image caches; plugins own content, layout and pointer interaction. */
typedef struct BongoCatPluginOverlayCanvas {
    uint32_t abi_version, struct_size;
    void *context;
    const char *resource_root;
    const char *(*text)(void *, const char *, const char *);
    uint32_t (*color)(void *, uint32_t);
    void (*rect)(void *, BongoCatPluginUiRect, float rounding, float stroke, uint32_t);
    void (*line)(void *, BongoCatPluginUiRect, float, uint32_t);
    void (*draw_text)(void *, BongoCatPluginUiRect, const char *, uint32_t font, uint32_t color, uint32_t align);
    float (*measure)(void *, const char *, uint32_t);
    uint32_t (*image)(void *, uint32_t slot, const char *, BongoCatPluginUiRect);
    void (*digest)(const void *, size_t, char *);
} BongoCatPluginOverlayCanvas;
typedef void (*BongoCatPluginOverlayText)(void *, const char *);
typedef struct BongoCatPluginOverlay {
    uint32_t abi_version, struct_size, width, height;
    const char *title_key, *title;
    uint32_t (*draw)(void *, const BongoCatPluginOverlayCanvas *);
    void (*pointer)(void *, float, float, uint32_t down);
    uint32_t (*visit_text)(void *, BongoCatPluginOverlayText, void *);
} BongoCatPluginOverlay;
typedef const BongoCatPluginOverlay *(*BongoCatPluginOverlayQuery)(uint32_t);
#endif
