#ifndef BONGO_CAT_RHI_CORNERS_H
#define BONGO_CAT_RHI_CORNERS_H
#include <SDL3/SDL.h>
#include "window_corner_policy.h"
/* Internal window properties keep the renderer/plugin ABI unchanged. */
static inline void bongo_cat_rhi_store_corners(SDL_Window *window, BongoCatCornerRect rect) {
    SDL_PropertiesID p = SDL_GetWindowProperties(window);
    SDL_SetNumberProperty(p, "BongoCat.CornerX", rect.x);
    SDL_SetNumberProperty(p, "BongoCat.CornerY", rect.y);
    SDL_SetNumberProperty(p, "BongoCat.CornerWidth", rect.width);
    SDL_SetNumberProperty(p, "BongoCat.CornerHeight", rect.height);
    SDL_SetNumberProperty(p, "BongoCat.CornerRadius", rect.radius_milli);
}
/* Native raster coordinates have their origin at the top-left. */
static inline bool bongo_cat_rhi_corner_params(SDL_Window *window,
    int width, int height, float params[8]) {
    SDL_PropertiesID p = SDL_GetWindowProperties(window);
    int x = (int)SDL_GetNumberProperty(p, "BongoCat.CornerX", 0);
    int y = (int)SDL_GetNumberProperty(p, "BongoCat.CornerY", 0);
    int cw = (int)SDL_GetNumberProperty(p, "BongoCat.CornerWidth", 0);
    int ch = (int)SDL_GetNumberProperty(p, "BongoCat.CornerHeight", 0);
    int radius = (int)SDL_GetNumberProperty(p, "BongoCat.CornerRadius", 0);
    if (radius <= 0 || cw <= 0 || ch <= 0 || x < 0 || y < 0 ||
        cw > width || ch > height || x > width - cw || y > height - ch) return false;
    params[0] = (float)x; params[1] = (float)(height - y - ch);
    params[2] = (float)cw; params[3] = (float)ch;
    params[4] = radius * 0.001f;
    params[5] = (float)width; params[6] = (float)height; params[7] = 0;
    return true;
}
#endif
