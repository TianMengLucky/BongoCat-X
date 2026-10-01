/* Runtime import of the user-supplied Cubism Core (Windows runtime-Core
   builds): load the DLL, swap the diagnostic stub for the Live2D bridge,
   and reload the active model without restarting the application. */
#include "runtime.h"
#include "bongo_cat/platform.h"

bool bongo_cat_app_import_live2d_core(BongoCatApp *app, const char *path,
    BongoCatError *error) {
    if (!app || !path || !path[0]) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_ARGUMENT,
            "Cannot import the Cubism Core without an application and file");
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
    /* The bridge needs a current OpenGL context when it starts its framework,
       so swap backends with the pet window's context bound. The previous
       instance is always a stub here because the Core was unavailable. */
    SDL_Window *previous_window = SDL_GL_GetCurrentWindow();
    SDL_GLContext previous_context = SDL_GL_GetCurrentContext();
    if (app->window && !SDL_GL_MakeCurrent(app->window, app->gl_context))
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
            "Cannot bind the pet window context for the Live2D swap: %s",
            SDL_GetError());
    bongo_cat_live2d_destroy(app->live2d);
    app->live2d = bongo_cat_live2d_create(app->asset_root, error);
    bool reloaded = app->live2d && (!app->loaded_model[0] ||
        bongo_cat_app_reload_model_with_error(app, error));
    if (previous_window && previous_context &&
        previous_window != app->window &&
        !SDL_GL_MakeCurrent(previous_window, previous_context))
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "Cannot restore the previous OpenGL context: %s", SDL_GetError());
    return reloaded;
}
