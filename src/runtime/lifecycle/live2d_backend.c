/* Runtime import of the user-supplied Cubism Core (runtime-Core builds):
   load the library, swap the diagnostic stub for the Live2D bridge, and
   reload the active model without restarting the application. */
#include "runtime.h"
#include "bongo_cat/platform.h"

/* The bridge needs a current OpenGL context when it starts its framework,
   so swap backends with the pet window's context bound. The previous
   instance is always a stub here because the Core was unavailable. */
static bool swap_live2d_backend(BongoCatApp *app, BongoCatError *error) {
    SDL_Window *previous_window = SDL_GL_GetCurrentWindow();
    SDL_GLContext previous_context = SDL_GL_GetCurrentContext();
    if (app->window && !bongo_cat_rhi_make_current(&app->rhi))
        SDL_LogWarn(SDL_LOG_CATEGORY_CUSTOM,
            "Cannot bind the pet window context for the Live2D swap: %s",
            SDL_GetError());
    bongo_cat_model_runtime_destroy(app->model_runtime);
    app->model_runtime = bongo_cat_model_runtime_create(app->asset_root, error);
    attach_rhi_info(app);
    bool reloaded = app->model_runtime && (!app->loaded_model[0] ||
        bongo_cat_app_reload_model_with_error(app, error));
    if (previous_window && previous_context &&
        previous_window != app->window &&
        !SDL_GL_MakeCurrent(previous_window, previous_context))
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM,
            "Cannot restore the previous OpenGL context: %s", SDL_GetError());
    return reloaded;
}

bool bongo_cat_app_import_live2d_core(BongoCatApp *app, const char *path,
    BongoCatError *error) {
    if (!app || !path || !path[0]) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_ARGUMENT,
            "Cannot import the Cubism Core without an application and file");
        return false;
    }
    if (!bongo_cat_rhi_live2d_supported(&app->rhi)) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "This build has no Live2D renderer for the selected backend");
        return false;
    }
    if (!bongo_cat_platform_live2d_core_import_supported()) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "This build cannot import the Cubism Core at runtime");
        return false;
    }
    if (bongo_cat_platform_live2d_core_available()) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "The Cubism Core is already loaded in this session");
        return false;
    }
    if (!bongo_cat_platform_live2d_core_import(path, app->data_root, error))
        return false;
    return swap_live2d_backend(app, error);
}

bool bongo_cat_app_activate_live2d_core(BongoCatApp *app, BongoCatError *error) {
    if (!app || !error) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_ARGUMENT,
            "Cannot activate the Cubism Core without an application");
        return false;
    }
    if (!bongo_cat_platform_live2d_core_available()) return false;
    SDL_LogInfo(SDL_LOG_CATEGORY_CUSTOM,
        "[live2d] Core available; swapping the Live2D backend in");
    return swap_live2d_backend(app, error);
}

bool bongo_cat_app_rescan_live2d_core(BongoCatApp *app) {
    if (!app) return false;
    if (bongo_cat_platform_live2d_core_available()) return true;
    if (!bongo_cat_platform_live2d_core_rescan(app->data_root)) return false;
    return bongo_cat_app_activate_live2d_core(app, &(BongoCatError){0});
}
