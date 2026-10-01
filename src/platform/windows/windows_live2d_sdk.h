#ifndef BONGO_CAT_WINDOWS_LIVE2D_SDK_H
#define BONGO_CAT_WINDOWS_LIVE2D_SDK_H

#ifdef __cplusplus
extern "C" {
#endif
#include "bongo_cat/common.h"
/* Locate and preload the runtime Cubism Core DLL (Live2DCubismCore.dll)
   before the Live2D backend initializes. Search order: the cached registry
   path, plain directories, then SDK zip archives dropped by the user, which
   are extracted on first sight. data_dir may be NULL or empty. Best effort:
   failures only leave Live2D rendering unavailable. */
void bongo_cat_windows_live2d_sdk_prepare(const char *data_dir);
bool bongo_cat_windows_live2d_sdk_ready(void);
/* Import a user-selected Core DLL or official SDK zip from the settings
   window: the file is copied or extracted into the data directory so later
   launches find it again, then loaded into this process immediately.
   Returns false with error set when the Core is already loaded or the file
   does not contain a usable Core. */
bool bongo_cat_windows_live2d_sdk_import(const char *path,
    const char *data_dir, BongoCatError *error);
#ifdef __cplusplus
}
#endif
#endif
