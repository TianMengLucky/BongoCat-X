#include "runtime.h"
#include "bongo_cat/path.h"
#include "bongo_cat/overlay.h"
#include "bongo_cat/i18n.h"
#include "bongo_cat/log.h"
#include "preferences_notice.h"

#include <SDL3/SDL_opengl.h>

void bongo_cat_window_clear_background(BongoCatApp *app) {
    BongoCatWindowPreferences *window = &app->settings.window;
    char path[BONGO_CAT_PATH_CAP];
    BongoCatError error = {0};
    const char *image = NULL;
    if (window->custom_background &&
        bongo_cat_path_join(path, sizeof(path), app->data_root, window->custom_background_path))
        image = path;
    bool loaded = bongo_cat_overlay_set_custom_background(app->overlay, image, &error);
    if (window->custom_background && (!image || !loaded)) {
        window->custom_background = false;
        app->dirty = true;
        bongo_cat_overlay_set_custom_background(app->overlay, NULL, NULL);
        SDL_LogWarn(BONGO_CAT_LOG_LIFECYCLE, "Background image load failed: %s", error.message);
        bongo_cat_preferences_notice_show(app, bongo_cat_i18n_get(app->i18n,
            "pages.preference.cat.background.failed", "Cannot load the background image"), true);
    }
    uint32_t rgb = window->obs_background_rgb;
    float alpha = window->obs_background ? 1.0f : 0.0f;
    bongo_cat_rhi_clear(&app->rhi, window->obs_background ? ((rgb >> 16) & 255) / 255.0f : 0.0f,
        window->obs_background ? ((rgb >> 8) & 255) / 255.0f : 0.0f,
        window->obs_background ? (rgb & 255) / 255.0f : 0.0f, alpha);
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
