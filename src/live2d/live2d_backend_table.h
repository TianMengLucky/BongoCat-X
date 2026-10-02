/* Internal to the Live2D backend: the API table the bridge fills and the
   plugin hands to the executable, plus the test-side installer entry point.
   Not an installed header. */
#ifndef BONGO_CAT_LIVE2D_BACKEND_TABLE_H
#define BONGO_CAT_LIVE2D_BACKEND_TABLE_H

#include "bongo_cat/live2d_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Defined by the compiled Cubism bridge sources (renamed to bridge_live2d_*
   at build time) and consumed by bongo_cat_live2d_backend_api(). */
extern const BongoCatLive2DBackendApi bongo_cat_live2d_backend_table;

/* Host services the bridge reaches through live2d_backend_sdl.h; the plugin
   installs the executable's table here in bongo_cat_live2d_backend_init(). */
const BongoCatLive2DBackendHost *bongo_cat_live2d_backend_host(void);

#ifdef __cplusplus
}
#endif

#endif
