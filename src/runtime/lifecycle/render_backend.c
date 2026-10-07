/* Rebuild the pet render stack at a frame boundary; settings, input and
   tray remain alive. A native model/device failure retries with OpenGL. */
#include "runtime.h"
#include "bongo_cat/log.h"
#include "bongo_cat/overlay.h"
#include "bongo_cat/preferences.h"
#include "bongo_cat/i18n.h"
#include <string.h>

static void release_stack(BongoCatApp *app) {
    if (app->gl_context) SDL_GL_MakeCurrent(app->window, app->gl_context);
    bongo_cat_rhi_wait_idle(&app->rhi);
    bongo_cat_live2d_destroy(app->live2d); app->live2d = NULL;
    bongo_cat_overlay_destroy(app->overlay); app->overlay = NULL;
    bongo_cat_window_close(app);
}
static bool create_stack(BongoCatApp *app, const char *model, BongoCatError *error) {
    if (bongo_cat_window_create(app, error) != BONGO_CAT_OK) return false;
    app->platform.present_ops = &app->rhi.present;
    bongo_cat_platform_window_replaced(&app->platform, app->window);
    app->startup_visibility_pending = app->session.window.visible;
    app->window_minimized = false;
    app->click_through_valid = false;
    app->render_retry_ns = 0;
    app->dirty = true;
    bongo_cat_window_apply(app);
    if (!bongo_cat_rhi_live2d_supported(&app->rhi)) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "This build has no Live2D renderer for the selected graphics API");
        return false;
    }
    app->live2d = bongo_cat_live2d_create(app->asset_root, error);
    if (!app->live2d) return false;
    attach_rhi_info(app);
    if (bongo_cat_rhi_is_gl(&app->rhi)) {
        BongoCatError optional = {0};
        app->overlay = bongo_cat_overlay_create(&optional);
        if (!app->overlay) SDL_LogWarn(BONGO_CAT_LOG_LIFECYCLE, "%s", optional.message);
    }
    // Fresh runtime has no model: avoid recapturing empty behavior state.
    app->loaded_model[0] = '\0';
    return !model[0] || bongo_cat_app_select_model_with_error(app, model, error);
}
bool bongo_cat_app_rebuild_render_backend(BongoCatApp *app, BongoCatError *error) {
    if (!app || !error) return false;
    SDL_Window *previous_window = SDL_GL_GetCurrentWindow();
    SDL_GLContext previous_context = SDL_GL_GetCurrentContext();
    bool restore = previous_window && previous_context && previous_window != app->window;
    bool native_failed = app->window && !bongo_cat_rhi_is_gl(&app->rhi) &&
        SDL_GetNumberProperty(SDL_GetWindowProperties(app->window), "BongoCat.NativeFailures", 0) >= 3;
    char model[BONGO_CAT_ID_CAP];
    SDL_strlcpy(model, app->loaded_model, sizeof(model));
    bongo_cat_app_capture_behavior_state(app);
    bongo_cat_window_snapshot_end(app);
    release_stack(app);
    BongoCatRenderBackend requested = app->settings.app.render_backend;
    bool ready = create_stack(app, model, error);
    if (!ready && requested != BONGO_CAT_RENDER_BACKEND_OPENGL &&
        requested != BONGO_CAT_RENDER_BACKEND_AUTO) {
        SDL_LogWarn(BONGO_CAT_LOG_LIFECYCLE, "Native renderer failed: %s; retrying OpenGL", error->message);
        release_stack(app);
        app->settings.app.render_backend = BONGO_CAT_RENDER_BACKEND_OPENGL;
        ready = create_stack(app, model, error);
    }
    if (ready && bongo_cat_rhi_is_gl(&app->rhi) && (native_failed ||
        (requested != BONGO_CAT_RENDER_BACKEND_AUTO && requested != BONGO_CAT_RENDER_BACKEND_OPENGL))) {
        app->settings.app.render_backend = BONGO_CAT_RENDER_BACKEND_OPENGL;
        bongo_cat_preferences_notice_show(app, bongo_cat_i18n_get(app->i18n,
            "pages.preference.general.hints.renderBackendFallback",
            "The selected renderer failed; restored OpenGL"), true);
    }
    if (restore) SDL_GL_MakeCurrent(previous_window, previous_context);
    if (!ready) app->running = false;
    else {
        memset(error, 0, sizeof(*error));
        app->last_frame_ns = SDL_GetTicksNS();
        bongo_cat_app_reset_pointer_tracking(app);
        bongo_cat_window_mark_hit_dirty(app);
        app->dirty = true;
        SDL_LogInfo(BONGO_CAT_LOG_LIFECYCLE, "[render] switched to %s", bongo_cat_rhi_describe(&app->rhi));
    }
    return ready;
}
