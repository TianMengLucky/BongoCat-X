/* Hot render-backend switch: rebuilds the render stack (window, RHI
   backend, overlay, Live2D) at a frame boundary without restarting the
   process. Tray, input receiver and the preferences window (which owns
   its own GL context) survive the swap; the Windows layered presenter is
   rebound through the platform window-replaced hook. Live2D stays on the
   OpenGL backend until its Vulkan/Metal draw phases land. */
#include "runtime.h"
#include "bongo_cat/log.h"
#include "bongo_cat/overlay.h"

#include <SDL3/SDL.h>

bool bongo_cat_app_rebuild_render_backend(BongoCatApp *app,
    BongoCatError *error) {
    if (!app) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_ARGUMENT,
            "Cannot rebuild the render stack without an application");
        return false;
    }
    /* The preferences window draws on its own GL context; restore it once
       the rebuild is done, like the Live2D core swap does. */
    SDL_Window *previous_window = SDL_GL_GetCurrentWindow();
    SDL_GLContext previous_context = SDL_GL_GetCurrentContext();
    bongo_cat_window_snapshot_end(app);
    /* GL object deletion must run against the owning context; bind the pet
       window's context like the Live2D core swap does. */
    if (app->window && app->gl_context &&
        !SDL_GL_MakeCurrent(app->window, app->gl_context))
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
            "Cannot bind the pet window context for the backend switch: %s",
            SDL_GetError());
    bongo_cat_live2d_destroy(app->live2d);
    app->live2d = NULL;
    bongo_cat_overlay_destroy(app->overlay);
    app->overlay = NULL;
    bongo_cat_window_close(app);
    if (bongo_cat_window_create(app, error) != BONGO_CAT_OK) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
            "Render backend switch failed while creating the window: %s",
            error->message);
        return false;
    }
    bongo_cat_platform_window_replaced(&app->platform, app->window);
    app->platform.present_ops = &app->rhi.present;
    bongo_cat_window_apply(app);
    bool gl = bongo_cat_rhi_is_gl(&app->rhi);
    if (gl) {
        app->live2d = bongo_cat_live2d_create(app->asset_root, error);
        if (!app->live2d) {
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                "Render backend switch failed while creating Live2D: %s",
                error->message);
            return false;
        }
        if (app->loaded_model[0] &&
            !bongo_cat_app_reload_model_with_error(app, error)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
                "The active model failed to reload after the backend switch: %s",
                error->message);
        }
    } else {
        SDL_LogWarn(BONGO_CAT_LOG_LIFECYCLE,
            "Live2D rendering is disabled on the %s backend until the "
            "draw phases port", bongo_cat_rhi_describe(&app->rhi));
    }
    BongoCatError overlay_error = {0};
    app->overlay = bongo_cat_overlay_create(&overlay_error);
    if (!app->overlay)
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Overlay disabled: %s",
            overlay_error.message);
    if (previous_window && previous_context &&
        previous_window != app->window &&
        !SDL_GL_MakeCurrent(previous_window, previous_context))
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
            "Cannot restore the previous OpenGL context: %s", SDL_GetError());
    SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO, "[render] switched to %s",
        bongo_cat_rhi_describe(&app->rhi));
    return true;
}
