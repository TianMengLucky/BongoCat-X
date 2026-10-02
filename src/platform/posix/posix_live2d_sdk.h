#ifndef BONGO_CAT_POSIX_LIVE2D_SDK_H
#define BONGO_CAT_POSIX_LIVE2D_SDK_H

#ifdef __cplusplus
extern "C" {
#endif
#include "bongo_cat/common.h"
/* Locate and preload the runtime Cubism Core shared library
   (libLive2DCubismCore.so / .dylib) before the Live2D backend initializes.
   Search order: a drop-in library beside the application, then the live2d
   folders next to the application and inside the data directory, including
   official SDK zip archives dropped by the user, which are extracted on
   first sight. data_dir may be NULL or empty. Best effort: failures only
   leave Live2D rendering unavailable. */
void bongo_cat_posix_live2d_sdk_prepare(const char *data_dir);
bool bongo_cat_posix_live2d_sdk_ready(void);
/* Import a user-selected Core library or official SDK zip from the settings
   window: the file is copied or extracted into the data directory so later
   launches find it again, then loaded into this process immediately.
   Returns false with error set when the Core is already loaded or the file
   does not contain a usable Core. */
bool bongo_cat_posix_live2d_sdk_import(const char *path,
    const char *data_dir, BongoCatError *error);
/* Re-run the discovery over the drop-in folders; true when a Core is
   loaded afterwards. */
bool bongo_cat_posix_live2d_sdk_rescan(const char *data_dir);
/* dlopen handle of the loaded Core, or NULL. Called by the generated Core
   shim; opens the library on first use when the startup scan has not run. */
void *bongo_cat_posix_live2d_core_library(void);
#ifdef __cplusplus
}
#endif
#endif
