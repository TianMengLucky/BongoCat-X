#include "runtime.h"
#include "modal_frame.h"

static int rounded_delta(float value) {
    return value < 0.0f ? (int)(value - 0.5f) : (int)(value + 0.5f);
}

static bool use_pointer_drag(const BongoCatApp *app) {
    (void)app;
    return true;
}

#define EDGE_BOUNCE_DURATION_NS 350000000ull
#define EDGE_BOUNCE_MIN_SPEED 500.0f
#define EDGE_BOUNCE_MAX_SPEED 2400.0f

static void sample_drag_velocity(BongoCatApp *app, float x, float y, uint64_t now) {
    uint64_t elapsed = now - app->drag_sample_ns;
    if (elapsed < 8000000ull) return;
    float seconds = (float)((double)elapsed / 1000000000.0);
    app->drag_velocity_x = (x - app->drag_sample_x) / seconds;
    app->drag_velocity_y = (y - app->drag_sample_y) / seconds;
    app->drag_sample_x = x; app->drag_sample_y = y;
    app->drag_sample_ns = now; app->drag_velocity_ns = now;
}

static bool start_edge_bounce(BongoCatApp *app, int x, int y, uint64_t now) {
    if (!app->settings.window.keep_in_screen ||
        !app->settings.window.screen_edge_bounce || !app->drag_velocity_ns ||
        now - app->drag_velocity_ns > 100000000ull) return false;
    int width, height;
    if (!SDL_GetWindowSize(app->window, &width, &height)) return false;
    SDL_Point fitted = {x, y};
    bongo_cat_window_constrain_position(app, &fitted, width, height);
    float vx = app->drag_velocity_x, vy = app->drag_velocity_y;
    bool hit_x = fitted.x != x && (x - (double)fitted.x) * vx > 0.0;
    bool hit_y = fitted.y != y && (y - (double)fitted.y) * vy > 0.0;
    if ((!hit_x || SDL_fabsf(vx) < EDGE_BOUNCE_MIN_SPEED) &&
        (!hit_y || SDL_fabsf(vy) < EDGE_BOUNCE_MIN_SPEED)) return false;
    /* Preserve the direction, reflecting only the collided axes. */
    float speed = SDL_sqrtf(vx * vx + vy * vy);
    float factor = speed > EDGE_BOUNCE_MAX_SPEED ? EDGE_BOUNCE_MAX_SPEED / speed : 1.0f;
    bongo_cat_window_drag_to(app, fitted.x, fitted.y);
    bongo_cat_window_drag_end(app);
    if (!SDL_GetWindowPosition(app->window, &app->edge_bounce_x,
        &app->edge_bounce_y)) return false;
    app->edge_bounce_velocity_x = (hit_x ? -vx : vx) * factor;
    app->edge_bounce_velocity_y = (hit_y ? -vy : vy) * factor;
    app->edge_bounce_start_ns = now;
    app->edge_bounce_end_ns = now + EDGE_BOUNCE_DURATION_NS;
    return true;
}

void bongo_cat_window_update_edge_bounce(BongoCatApp *app, uint64_t now) {
    if (!app || !app->edge_bounce_end_ns) return;
    if (!app->settings.window.keep_in_screen || !app->settings.window.screen_edge_bounce ||
        !app->session.window.visible || app->window_minimized || app->hover_hidden ||
        app->settings.window.pass_through || app->resize_gesture || app->resize_candidate ||
        app->wheel_animation_active || app->window_drag_active) {
        app->edge_bounce_end_ns = 0; return;
    }
    uint64_t time = SDL_min(now, app->edge_bounce_end_ns);
    float seconds = (float)((double)(time - app->edge_bounce_start_ns) / 1000000000.0);
    SDL_Point requested = {
        app->edge_bounce_x + rounded_delta(app->edge_bounce_velocity_x * seconds),
        app->edge_bounce_y + rounded_delta(app->edge_bounce_velocity_y * seconds)};
    SDL_Point fitted = requested;
    int width, height;
    if (!SDL_GetWindowSize(app->window, &width, &height)) {
        app->edge_bounce_end_ns = 0; return;
    }
    bongo_cat_window_constrain_position(app, &fitted, width, height);
    /* Bypass edge snapping: it would hold a rebounding pet against the edge. */
    if (!SDL_SetWindowPosition(app->window, fitted.x, fitted.y)) {
        app->edge_bounce_end_ns = 0; return;
    }
    app->session.window.x = fitted.x; app->session.window.y = fitted.y;
    app->session.window.position_known = true;
    bongo_cat_window_mark_hit_dirty(app);
    app->dirty = true;
    if (fitted.x != requested.x || fitted.y != requested.y) {
        if (fitted.x != requested.x) app->edge_bounce_velocity_x *= -1.0f;
        if (fitted.y != requested.y) app->edge_bounce_velocity_y *= -1.0f;
        app->edge_bounce_x = fitted.x; app->edge_bounce_y = fitted.y;
        app->edge_bounce_start_ns = time;
    }
    if (now >= app->edge_bounce_end_ns) app->edge_bounce_end_ns = 0;
}

static void move_with_pointer(BongoCatApp *app, float x, float y) {
    int next_x = app->drag_window_x + rounded_delta(x - app->drag_start_x);
    int next_y = app->drag_window_y + rounded_delta(y - app->drag_start_y);
    uint64_t now = SDL_GetTicksNS();
    sample_drag_velocity(app, x, y, now);
    if (!start_edge_bounce(app, next_x, next_y, now))
        bongo_cat_window_drag_to(app, next_x, next_y);
}

void bongo_cat_window_drag_begin(BongoCatApp *app,
    const SDL_MouseButtonEvent *event) {
    if (!app || !event) return;
    bongo_cat_window_cancel_wheel_animation(app);
    app->drag_candidate = bongo_cat_window_visible_at_pointer(
        app, event->x, event->y);
    if (!app->drag_candidate || !SDL_GetWindowPosition(app->window,
        &app->drag_window_x, &app->drag_window_y)) {
        app->drag_candidate = false; return;
    }
    app->edge_bounce_end_ns = 0;
    SDL_GetGlobalMouseState(&app->drag_start_x, &app->drag_start_y);
    app->drag_sample_x = app->drag_start_x; app->drag_sample_y = app->drag_start_y;
    app->drag_sample_ns = SDL_GetTicksNS();
    app->drag_velocity_ns = 0;
}

void bongo_cat_window_drag_motion(BongoCatApp *app,
    const SDL_MouseMotionEvent *event) {
    if (!app || !event || (!app->drag_candidate && !app->window_drag_active)) return;
    float pointer_x = 0.0f, pointer_y = 0.0f;
    SDL_MouseButtonFlags buttons = SDL_GetGlobalMouseState(&pointer_x, &pointer_y);
    if (!(buttons & SDL_BUTTON_LMASK)) {
        bongo_cat_window_drag_end(app); return;
    }
    if (app->window_drag_active) {
        if (use_pointer_drag(app))
            move_with_pointer(app, pointer_x, pointer_y);
        return;
    }
    float x = pointer_x - app->drag_start_x;
    float y = pointer_y - app->drag_start_y;
    if (x * x + y * y < 9.0f) return;
    app->drag_candidate = false;
    app->window_drag_active = true;
    bongo_cat_window_snapshot_begin(app);
    bongo_cat_window_drag_bounds_refresh(app);
    if (use_pointer_drag(app)) {
        if (!SDL_CaptureMouse(true)) SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
            "Mouse capture is unavailable during window drag: %s",
            SDL_GetError());
        move_with_pointer(app, pointer_x, pointer_y);
    } else {
        BongoCatModalFrame modal_frame;
        bongo_cat_modal_frame_init(&modal_frame, app);
        bongo_cat_platform_begin_drag(&app->platform,
            bongo_cat_modal_frame_tick, &modal_frame);
    }
}

void bongo_cat_window_drag_end(BongoCatApp *app) {
    if (!app) return;
    bool was_active = app->window_drag_active;
    if (was_active) SDL_CaptureMouse(false);
    app->drag_candidate = false;
    app->window_drag_active = false;
    bongo_cat_window_snapshot_end(app);
    bongo_cat_window_drag_bounds_clear(app);
    if (was_active && app->settings.model.mouse_centered) {
        float pointer_x = 0.0f, pointer_y = 0.0f;
        SDL_GetGlobalMouseState(&pointer_x, &pointer_y);
        bongo_cat_app_apply_mouse_position(app, pointer_x, pointer_y, 0.0f);
    }
}
