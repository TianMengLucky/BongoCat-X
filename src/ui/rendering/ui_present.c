#include "ui_present.h"
#ifdef _WIN32
#include "windows_hdr.h"
#endif

bool bongo_cat_ui_present(SDL_Window *window) {
    if (!window) return false;
    SDL_Renderer *software = SDL_GetPointerProperty(SDL_GetWindowProperties(window), "BongoCat.SoftwareUIRenderer", NULL);
    if (software) return SDL_RenderPresent(software);
#ifdef _WIN32
    int width, height;
    if (!SDL_GetWindowSizeInPixels(window, &width, &height)) return false;
    return bongo_cat_windows_hdr_present(window, width, height);
#else
    return SDL_GL_SwapWindow(window);
#endif
}
