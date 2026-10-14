#ifndef BONGO_CAT_WINDOWS_COMPOSITION_H
#define BONGO_CAT_WINDOWS_COMPOSITION_H
#ifdef _WIN32
#include <stdbool.h>
#include <windows.h>
#ifdef __cplusplus
extern "C" {
#endif
bool bongo_cat_windows_composition_present(void **state, HWND window,
    const RECT *bounds, const unsigned char *pixels, int width, int height, float opacity);
void bongo_cat_windows_composition_destroy(void *state);
#ifdef __cplusplus
}
#endif
#endif
#endif
