#ifndef BONGO_CAT_WINDOWS_TASKBAR_H
#define BONGO_CAT_WINDOWS_TASKBAR_H

#ifdef _WIN32
#include <windows.h>
#ifdef __cplusplus
extern "C" {
#endif

HRESULT bongo_cat_windows_taskbar_remove(HWND window);

#ifdef __cplusplus
}
#endif
#endif
#endif
