#ifndef BONGO_CAT_LIVE2D_DISPATCH_H
#define BONGO_CAT_LIVE2D_DISPATCH_H

/* Internal boundary used by the runtime loader to install the Live2D backend
   shared library's API table into the dispatch layer. Not part of the public
   headers: only bongo_cat_runtime sources and the live2d test bridge use it. */
#include "bongo_cat/live2d_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

void bongo_cat_live2d_dispatch_install(const BongoCatLive2DBackendApi *api);
bool bongo_cat_live2d_dispatch_installed(void);

#ifdef __cplusplus
}
#endif
#endif
