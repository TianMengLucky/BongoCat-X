#include "runtime.h"

#include <SDL3/SDL_opengl.h>

void bongo_cat_window_clear_background(BongoCatApp *app) {
    BongoCatWindowPreferences *window = &app->settings.window;
    uint32_t rgb = window->obs_background_rgb;
    float alpha = window->obs_background ? 1.0f : 0.0f;
    glClearColor(window->obs_background ? ((rgb >> 16) & 255) / 255.0f : 0.0f,
        window->obs_background ? ((rgb >> 8) & 255) / 255.0f : 0.0f,
        window->obs_background ? (rgb & 255) / 255.0f : 0.0f, alpha);
    glClear(GL_COLOR_BUFFER_BIT);
    static int last_enabled = -1;
    static uint32_t last_color;
    if (last_enabled != (window->obs_background ? 1 : 0) ||
        last_color != window->obs_background_rgb) {
        last_enabled = window->obs_background;
        last_color = window->obs_background_rgb;
        SDL_Log("OBS background mode: enabled=%d color=#%06x",
            window->obs_background, (unsigned)last_color);
    }
}
