#include "runtime.h"
#include <SDL3/SDL_opengl.h>
#include <stdio.h>

static BongoCatRhiBackend resolve_rhi_backend(BongoCatRenderBackend setting) {
    switch (setting) {
    case BONGO_CAT_RENDER_BACKEND_VULKAN: return BONGO_CAT_RHI_VULKAN;
    case BONGO_CAT_RENDER_BACKEND_METAL: return BONGO_CAT_RHI_METAL;
    case BONGO_CAT_RENDER_BACKEND_OPENGL:
    case BONGO_CAT_RENDER_BACKEND_AUTO:
    default: return BONGO_CAT_RHI_OPENGL;
    }
}

BongoCatResult bongo_cat_window_create(BongoCatApp *app, BongoCatError *error) {
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1", SDL_HINT_OVERRIDE);
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
#ifdef _WIN32
    /* The native input thread owns keyboard Raw Input for this process. */
    SDL_SetHintWithPriority(SDL_HINT_WINDOWS_RAW_KEYBOARD, "0", SDL_HINT_OVERRIDE);
    /* Transparent OpenGL windows must never receive SDL's default black
       WM_ERASEBKGND fill before the first frame is submitted. */
    SDL_SetHint(SDL_HINT_WINDOWS_ERASE_BACKGROUND_MODE, "0");
#endif
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "SDL initialization failed: %s", SDL_GetError());
        return BONGO_CAT_ERROR_PLATFORM;
    }
    /* The dispatcher falls back to the OpenGL compatibility ladder when the
       selected backend is unavailable or fails to initialize. */
    BongoCatResult result = bongo_cat_rhi_create_window(
        resolve_rhi_backend(app->settings.app.render_backend),
        BONGO_CAT_PET_WINDOW_TITLE, app->session.window.width,
        app->session.window.height, true, &app->window, &app->rhi, error);
    if (result != BONGO_CAT_OK) return result;
    app->gl_context = app->rhi.backend == BONGO_CAT_RHI_OPENGL ?
        app->rhi.context : NULL;
    return BONGO_CAT_OK;
}

void bongo_cat_window_apply(BongoCatApp *app) {
    BongoCatWindowPreferences *preferences = &app->settings.window;
    BongoCatWindowState *state = &app->session.window;
    bongo_cat_app_cancel_hover_fade(app);
    bongo_cat_platform_set_opacity(&app->platform,
        state->opacity_percent / 100.0f);
    SDL_SetWindowSize(app->window, state->width, state->height);
    if (state->position_known)
        SDL_SetWindowPosition(app->window, state->x, state->y);
    SDL_SyncWindow(app->window);
    SDL_SyncWindow(app->window);
    /* A visible session is revealed by the first successful frame. Keeping
       the native window hidden while loading avoids exposing an uninitialised
       (and on some drivers black) back buffer. */
    bongo_cat_platform_set_visible(&app->platform,
        state->visible && !app->startup_visibility_pending);
    bongo_cat_window_sync_click_through(app);
    bongo_cat_platform_set_always_on_top(&app->platform,
        preferences->always_on_top);
    /* 只在录屏软件里显示 (Windows: DWM 隐藏)。放在最后: 上面几个调用都会动窗口
       样式/位置, 重新应用一次能保证隐藏状态不会在它们之后丢失。 */
    bongo_cat_platform_set_capture_only(&app->platform,
        app->settings.window.capture_only);
}

void bongo_cat_window_apply_capture_only(BongoCatApp *app) {
    if (!app || !app->window) return;
    if (!bongo_cat_platform_capture_only_supported()) {
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
            "仅在录屏软件中显示在当前平台不可用, 该设置已忽略");
        return;
    }
    if (!bongo_cat_platform_set_capture_only(&app->platform,
            app->settings.window.capture_only)) return;
    /* 桌面不可见时点击穿透必须整体生效: 让下一帧重新下发一次点击穿透状态
       (平台侧在 capture_only 打开时会强制整体穿透, 不再做逐像素命中测试)。 */
    app->click_through_valid = false;
    bongo_cat_window_sync_click_through(app);
    app->dirty = true;
}

static bool event_targets_main_window(BongoCatApp *app,
    const SDL_Event *event) {
    SDL_WindowID id = SDL_GetWindowID(app->window);
    if (event->type >= SDL_EVENT_WINDOW_FIRST &&
        event->type <= SDL_EVENT_WINDOW_LAST)
        return event->window.windowID == id;
    switch (event->type) {
    case SDL_EVENT_MOUSE_MOTION:
        return event->motion.windowID == id || app->window_drag_active ||
            app->drag_candidate || app->resize_candidate || app->resize_gesture;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        return event->button.windowID == id;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        return event->button.windowID == id || app->window_drag_active ||
            app->drag_candidate || app->resize_candidate || app->resize_gesture;
    case SDL_EVENT_MOUSE_WHEEL:
        return event->wheel.windowID == id;
    default:
        return true;
    }
}

bool bongo_cat_window_event(BongoCatApp *app, const SDL_Event *event) {
    bongo_cat_window_display_event(app, event);
    if (!event_targets_main_window(app, event)) return true;
    if (event->type == SDL_EVENT_QUIT) return false;
    if (event->type == SDL_EVENT_WINDOW_HIDDEN ||
        event->type == SDL_EVENT_WINDOW_MINIMIZED ||
        event->type == SDL_EVENT_WINDOW_FOCUS_LOST)
        bongo_cat_window_resize_end(app);
    if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        bongo_cat_window_set_visible(app, false);
        return true;
    }
    if (event->type == SDL_EVENT_WINDOW_HIDDEN) {
        bongo_cat_window_drag_end(app);
    }
    if (event->type == SDL_EVENT_WINDOW_MINIMIZED) {
        app->window_minimized = true;
        bongo_cat_window_drag_end(app);
    }
    if (event->type == SDL_EVENT_WINDOW_RESIZED) {
        /* Queued notifications may describe an earlier animation frame. */
        int width = app->session.window.width, height = app->session.window.height;
        SDL_GetWindowSize(app->window, &width, &height);
        if (width != app->session.window.width || height != app->session.window.height) {
            app->session.window.width = width;
            app->session.window.height = height;
            bongo_cat_window_content_size(app, width,
                height, &app->session.window.content_width,
                &app->session.window.content_height);
            app->dirty = true;
        }
    }
    if (event->type == SDL_EVENT_WINDOW_EXPOSED ||
        event->type == SDL_EVENT_WINDOW_HDR_STATE_CHANGED ||
        event->type == SDL_EVENT_WINDOW_SHOWN ||
        event->type == SDL_EVENT_WINDOW_RESTORED) {
        /* An expose can arrive without a restore on XWayland, but a queued
           expose is not proof that the window is currently restored. */
        app->window_minimized =
            (SDL_GetWindowFlags(app->window) & SDL_WINDOW_MINIMIZED) != 0;
        if (event->type != SDL_EVENT_WINDOW_EXPOSED)
            bongo_cat_app_reset_pointer_tracking(app);
        /* DWM can discard the transparent redirection surface after an
           Explorer/display refresh. Repaint even when the model is idle so
           the restored alpha surface is submitted immediately. */
        app->dirty = true;
        /* XWayland may drop _NET_WM_STATE_ABOVE while the surface is hidden
           and does not restore it when the window is shown again. Reapply the
           persisted preference after the surface has been exposed so startup
           and hide/show cycles retain the user's always-on-top choice. */
        if (event->type == SDL_EVENT_WINDOW_EXPOSED ||
            event->type == SDL_EVENT_WINDOW_SHOWN ||
            event->type == SDL_EVENT_WINDOW_RESTORED)
            bongo_cat_platform_set_always_on_top(&app->platform,
                app->settings.window.always_on_top);
    }
    if (event->type == SDL_EVENT_WINDOW_FOCUS_GAINED ||
        event->type == SDL_EVENT_WINDOW_FOCUS_LOST) {
        bongo_cat_app_reset_pointer_tracking(app);
    }
    if (event->type == SDL_EVENT_WINDOW_RESIZED ||
        event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        int width = 0, height = 0;
        if (SDL_GetWindowSizeInPixels(app->window, &width, &height) &&
            (width != app->resize_pixel_width || height != app->resize_pixel_height)) {
            app->resize_pixel_width = width;
            app->resize_pixel_height = height;
            app->resize_pending = true;
            app->dirty = true;
            bongo_cat_window_mark_hit_dirty(app);
        }
    } else if (event->type == SDL_EVENT_WINDOW_MOVED) {
        int x = 0, y = 0;
        if (SDL_GetWindowPosition(app->window, &x, &y) &&
            (!app->session.window.position_known ||
             x != app->session.window.x || y != app->session.window.y)) {
            app->session.window.x = x;
            app->session.window.y = y;
            app->session.window.position_known = true;
            app->pointer_known = false;
            bongo_cat_window_mark_hit_dirty(app);
        }
    } else if (event->type == SDL_EVENT_WINDOW_DISPLAY_CHANGED ||
        event->type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) {
        bongo_cat_app_reset_pointer_tracking(app);
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
        event->button.button == SDL_BUTTON_LEFT) {
        if (!app->resize_candidate && !app->resize_gesture)
            bongo_cat_window_drag_begin(app, &event->button);
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
        event->button.button == SDL_BUTTON_RIGHT) {
        bongo_cat_window_resize_begin(app, &event->button);
    } else if (event->type == SDL_EVENT_MOUSE_MOTION) {
        bongo_cat_window_resize_by_pointer(app, event);
        bongo_cat_window_drag_motion(app, &event->motion);
    } else if (event->type == SDL_EVENT_MOUSE_WHEEL) {
        bongo_cat_window_wheel(app, &event->wheel);
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_UP &&
        event->button.button == SDL_BUTTON_LEFT) {
        bongo_cat_window_mark_hit_dirty(app);
        bongo_cat_window_drag_end(app);
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_UP &&
        event->button.button == SDL_BUTTON_RIGHT) {
        bongo_cat_window_mark_hit_dirty(app);
        bool show_menu = app->resize_menu_pending && !app->resize_gesture;
        bongo_cat_window_resize_end(app);
        if (show_menu) bongo_cat_window_show_context_menu(app);
    }
    return true;
}

void bongo_cat_window_close(BongoCatApp *app) {
    bongo_cat_window_resize_end(app);
    bongo_cat_window_drag_end(app);
    if (app->gl_context && SDL_GL_MakeCurrent(app->window, app->gl_context))
        bongo_cat_window_destroy_corner_mask();
    /* Releases the backend device/surface state before the SDL window and
       the Vulkan loader go away; the GL context stays runtime-owned. */
    bongo_cat_rhi_destroy(&app->rhi);
    if (app->gl_context) SDL_GL_DestroyContext(app->gl_context);
    /* Detach the platform before the SDL window dies: presenters resolve
       their native handle through it, and the hot render-backend switch
       closes a window that platform_shutdown would otherwise still see. */
    app->platform.window = NULL;
    if (app->window) SDL_DestroyWindow(app->window);
    app->gl_context = NULL;
    app->window = NULL;
}

void bongo_cat_window_destroy(BongoCatApp *app) {
    bongo_cat_window_close(app);
    SDL_Quit();
}
