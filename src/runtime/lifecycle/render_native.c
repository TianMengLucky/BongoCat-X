#include "runtime.h"
#include "model_cover.h"
#include "bongo_cat/memory_policy.h"
#include "bongo_cat/log.h"

static bool draw_model(void *userdata) {
    BongoCatApp *app = userdata;
    attach_rhi_info(app);
    if (!app->loaded_model[0]) return true;
    if (!bongo_cat_live2d_ready(app->live2d))
        return !bongo_cat_platform_live2d_core_available();
    return bongo_cat_live2d_draw_checked(app->live2d);
}
bool bongo_cat_app_render_native(BongoCatApp *app, bool present) {
    uint64_t now = SDL_GetTicksNS();
    if (app->render_retry_ns > now) return false;
    if (!bongo_cat_rhi_make_current(&app->rhi)) goto failed;
    bongo_cat_window_apply_pending_resize(app);
    bongo_cat_live2d_set_vertical_flip(app->live2d, app->settings.model.vertical_flip);
    bongo_cat_live2d_set_mirror(app->live2d, app->settings.model.mirror);
    bool cover = !present && bongo_cat_model_cover_pending(app);
    bool cover_ready = !cover || bongo_cat_live2d_prepare_cover_capture(app->live2d);
    bongo_cat_window_update_model_frame(app);
    bongo_cat_window_apply_pending_resize(app);
    int width = 0, height = 0;
    if (!SDL_GetWindowSizeInPixels(app->window, &width, &height) || width <= 0 || height <= 0)
        return false;
    bongo_cat_rhi_prepare_frame(&app->rhi, width, height);
    bongo_cat_window_clear_background(app);
    app->rhi.present.draw_frame = draw_model;
    app->rhi.present.hook_user = app;
    if (!bongo_cat_rhi_render_frame(&app->rhi)) goto failed;
    app->render_retry_ns = 0;
    if (cover) {
        if (!cover_ready || !bongo_cat_model_cover_capture(app, width, height)) {
            bongo_cat_model_cover_defer(app, "native-frame-capture");
            return false;
        }
    }
    if (!present) {
        SDL_SetNumberProperty(SDL_GetWindowProperties(app->window), "BongoCat.NativeFailures", 0);
        app->dirty = true;
        return true;
    }
    bongo_cat_window_capture_pointer_hit(app, true);
    bool reveal = app->startup_visibility_pending && app->session.window.visible;
    if (reveal) bongo_cat_platform_set_visible(&app->platform, true);
    if (!bongo_cat_platform_present(&app->platform, width, height)) {
        if (reveal) bongo_cat_platform_set_visible(&app->platform, false);
        goto failed;
    }
    SDL_SetNumberProperty(SDL_GetWindowProperties(app->window), "BongoCat.NativeFailures", 0);
    app->startup_visibility_pending = false;
    app->input_diagnostics.presented_frames++;
    bongo_cat_startup_ready(app);
    bongo_cat_memory_policy_frame_presented();
    app->dirty = false;
    bongo_cat_window_sync_click_through(app);
    bongo_cat_window_schedule_hit_check(app);
    return true;
failed:
    app->dirty = true;
    app->render_retry_ns = now + 1000000000ull;
    SDL_LogError(BONGO_CAT_LOG_LIFECYCLE, "Native render/presentation failed: %s", SDL_GetError());
    // Driver or renderer failures recover at the next safe frame boundary.
    SDL_PropertiesID properties = SDL_GetWindowProperties(app->window);
    Sint64 failures = SDL_GetNumberProperty(properties, "BongoCat.NativeFailures", 0) + 1;
    SDL_SetNumberProperty(properties, "BongoCat.NativeFailures", failures);
    if (failures >= 3) {
        app->settings.app.render_backend = BONGO_CAT_RENDER_BACKEND_OPENGL;
        app->render_backend_swap_pending = true;
    }
    return false;
}
