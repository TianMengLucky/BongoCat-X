#ifndef BONGO_CAT_RUNTIME_PLUGIN_OVERLAY_H
#define BONGO_CAT_RUNTIME_PLUGIN_OVERLAY_H
#include "bongo_cat/plugin_overlay.h"
#include "bongo_cat/i18n.h"
#include "ui_backend.h"
#define BONGO_CAT_PLUGIN_OVERLAY_IMAGES 8u
typedef struct BongoCatPluginOverlaySurface {
    BongoCatUIBackend *ui;
    const BongoCatI18n *i18n;
    bool dark;
    unsigned textures[BONGO_CAT_PLUGIN_OVERLAY_IMAGES];
    char paths[BONGO_CAT_PLUGIN_OVERLAY_IMAGES][BONGO_CAT_PATH_CAP];
} BongoCatPluginOverlaySurface;
typedef struct BongoCatPluginOverlayGlyphs {
    nk_rune *ranges;
    size_t capacity;
    uint64_t hash;
} BongoCatPluginOverlayGlyphs;
const BongoCatPluginOverlay *bongo_cat_plugin_overlay_query(SDL_SharedObject *, const char *);
BongoCatPluginOverlayCanvas bongo_cat_plugin_overlay_canvas(BongoCatPluginOverlaySurface *, const char *);
void bongo_cat_plugin_overlay_release(BongoCatPluginOverlaySurface *);
void bongo_cat_plugin_overlay_text(void *, const char *);
#endif
