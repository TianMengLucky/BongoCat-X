#include "runtime.h"
#include "test.h"

#include <limits.h>
#include <string.h>

int bongo_cat_test_failures;
static SDL_Rect test_bounds[32];
static int display_count, display_queries, position_calls, snapshot_updates;
static int window_x, window_y, window_w, window_h;
static bool fail_displays, fail_position, fail_usable;

static SDL_DisplayID *display_test_list(int *count) {
    ++display_queries;
    *count = display_count;
    if (fail_displays) return NULL;
    SDL_DisplayID *ids = SDL_malloc(sizeof(*ids) * (size_t)(display_count + 1));
    if (!ids) return NULL;
    for (int i = 0; i < display_count; ++i) ids[i] = (SDL_DisplayID)(i + 1);
    ids[display_count] = 0;
    return ids;
}
static bool display_test_bounds(SDL_DisplayID id, SDL_Rect *rect) {
    if (!id || id > (SDL_DisplayID)display_count) return false;
    *rect = test_bounds[id - 1]; return true;
}
static bool usable_bounds(SDL_DisplayID id, SDL_Rect *rect) {
    return !fail_usable && display_test_bounds(id, rect);
}
static SDL_DisplayID primary_display(void) { return display_count ? 1 : 0; }
static SDL_DisplayID display_for_rect(const SDL_Rect *rect) {
    (void)rect; return primary_display();
}
static bool position(SDL_Window *window, int *x, int *y) {
    (void)window; *x = window_x; *y = window_y; return true;
}
static bool size(SDL_Window *window, int *w, int *h) {
    (void)window; *w = window_w; *h = window_h; return true;
}
static bool set_position(SDL_Window *window, int x, int y) {
    (void)window; ++position_calls;
    if (fail_position) return false;
    window_x = x; window_y = y; return true;
}
static uint64_t time_ns(void) { return 1000; }

void bongo_cat_window_mark_hit_dirty(BongoCatApp *app) { app->pointer_hit_dirty = true; }
void bongo_cat_window_snapshot_geometry(BongoCatApp *app, const SDL_FRect *rect) {
    (void)app; (void)rect; ++snapshot_updates;
}
void bongo_cat_window_snapshot_end(BongoCatApp *app) { (void)app; }
void bongo_cat_window_cancel_wheel_animation(BongoCatApp *app) { (void)app; }
void bongo_cat_app_reset_pointer_tracking(BongoCatApp *app) {
    app->pointer_known = false; app->model_pointer_anchor_ready = false;
    app->dirty = true;
}

#define SDL_GetDisplays display_test_list
#define SDL_GetDisplayBounds display_test_bounds
#define SDL_GetDisplayUsableBounds usable_bounds
#define SDL_GetPrimaryDisplay primary_display
#define SDL_GetDisplayForRect display_for_rect
#define SDL_GetWindowPosition position
#define SDL_GetWindowSize size
#define SDL_SetWindowPosition set_position
#define SDL_GetTicksNS time_ns
#include "../../src/runtime/shell/window_display.c"

static void reset(BongoCatApp *app) {
    memset(app, 0, sizeof(*app));
    app->window = (SDL_Window *)app;
    window_x = 100; window_y = 100; window_w = 320; window_h = 240;
    display_count = 2; display_queries = position_calls = snapshot_updates = 0;
    fail_displays = fail_position = fail_usable = false;
    test_bounds[0] = (SDL_Rect){0, 0, 1920, 1040};
    test_bounds[1] = (SDL_Rect){1920, 0, 1920, 1040};
}

static void check_fit(SDL_Rect requested, const SDL_Rect *screens, int count,
    int expected_x, int expected_y) {
    SDL_Point next = constrained_position(&requested, screens, count);
    CHECK(next.x == expected_x && next.y == expected_y);
}

static void test_coverage(void) {
    const SDL_Rect adjacent[] = {{0, 0, 1920, 1040}, {1920, 0, 1920, 1040}};
    const SDL_Rect gapped[] = {{0, 0, 1920, 1040}, {2020, 0, 1920, 1040}};
    const SDL_Rect negative[] = {{-1920, 0, 1920, 1080}, {0, 0, 2560, 1440}};
    const SDL_Rect stacked[] = {{0, -1200, 1920, 1200}, {0, 0, 1920, 1080},
        {1920, 0, 1280, 1024}};
    check_fit((SDL_Rect){-80, 100, 320, 240}, adjacent, 2, 0, 100);
    check_fit((SDL_Rect){1800, 100, 320, 240}, adjacent, 2, 1800, 100);
    check_fit((SDL_Rect){1800, 100, 320, 240}, gapped, 2, 1600, 100);
    check_fit((SDL_Rect){1980, 100, 320, 240}, gapped, 2, 2020, 100);
    check_fit((SDL_Rect){2200, 100, 320, 240}, adjacent, 2, 2200, 100);
    check_fit((SDL_Rect){2200, 100, 320, 240}, adjacent, 1, 1600, 100);
    check_fit((SDL_Rect){-100, 100, 200, 200}, negative, 2, -100, 100);
    check_fit((SDL_Rect){100, -100, 200, 200}, stacked, 3, 100, -100);
    check_fit((SDL_Rect){1800, -100, 240, 200}, stacked, 3, 1800, 0);
    check_fit((SDL_Rect){1800, 900, 320, 240}, adjacent, 2, 1800, 800);
    check_fit((SDL_Rect){1800, 100, 2500, 240}, adjacent, 2, 1340, 100);
    check_fit((SDL_Rect){-100, 100, 2200, 240}, adjacent, 1, -100, 100);
    check_fit((SDL_Rect){-500, 100, 2200, 240}, adjacent, 1, -280, 100);
    check_fit((SDL_Rect){INT_MAX, INT_MAX, 320, 240}, adjacent, 2, 3520, 800);
    check_fit((SDL_Rect){INT_MIN, INT_MIN, 320, 240}, adjacent, 2, 0, 0);
    check_fit((SDL_Rect){-80, 100, 320, 240}, NULL, 0, -80, 100);
}

static void test_drag(BongoCatApp *app) {
    reset(app);
    bongo_cat_window_drag_to(app, -80, 100);
    CHECK(window_x == -80 && display_queries == 0);
    app->settings.window.keep_in_screen = true;
    app->window_drag_active = true;
    bongo_cat_window_drag_bounds_refresh(app);
    CHECK(display_queries == 1 && app->drag_display_count == 2);
    bongo_cat_window_drag_to(app, -80, 100);
    CHECK(window_x == 0 && window_y == 100 && display_queries == 1);
    bongo_cat_window_drag_to(app, 1800, 100);
    CHECK(window_x == 1800 && display_queries == 1);
    int calls = position_calls;
    bongo_cat_window_drag_to(app, 1800, 100);
    CHECK(position_calls == calls);
    fail_position = true;
    bongo_cat_window_drag_to(app, 2000, 100);
    CHECK(window_x == 1800 && app->session.window.x == 1800);
    bongo_cat_window_drag_bounds_clear(app);
    CHECK(!app->drag_display_bounds && app->drag_display_count == 0);
}

static void test_snap(BongoCatApp *app) {
    reset(app);
    app->settings.window.edge_snap = true;
    app->window_drag_active = true;
    bongo_cat_window_drag_bounds_refresh(app);
    bongo_cat_window_drag_to(app, -10, 100);
    CHECK(window_x == 0 && window_y == 100);
    bongo_cat_window_drag_to(app, 10, 2000);
    CHECK(window_x == 10 && window_y == 2000);
    bongo_cat_window_drag_to(app, 5000, 10);
    CHECK(window_x == 5000 && window_y == 10);
    app->settings.window.keep_in_screen = true;
    bongo_cat_window_drag_to(app, -40, 900);
    CHECK(window_x == 0 && window_y == 800);
    app->settings.window.edge_snap = false;
    bongo_cat_window_drag_to(app, 10, 100);
    CHECK(window_x == 10);
    bongo_cat_window_drag_bounds_clear(app);
}

static void test_clamp_and_recovery(BongoCatApp *app) {
    reset(app);
    app->settings.window.keep_in_screen = true;
    window_x = -80;
    app->window_snapshot = (void *)app;
    bongo_cat_window_clamp_to_display(app);
    CHECK(window_x == 0 && app->session.window.position_known && app->dirty);
    CHECK(app->pointer_hit_dirty && snapshot_updates == 1);
    int calls = position_calls;
    bongo_cat_window_clamp_to_display(app);
    CHECK(position_calls == calls && snapshot_updates == 1);
    window_x = -80; fail_position = true;
    bongo_cat_window_clamp_to_display(app);
    CHECK(window_x == -80 && app->session.window.x == 0);
    fail_position = false; fail_displays = true;
    bongo_cat_window_clamp_to_display(app);
    CHECK(window_x == -80);
    fail_displays = false; fail_usable = true;
    bongo_cat_window_clamp_to_display(app);
    CHECK(window_x == 0);

    reset(app);
    app->settings.window.keep_in_screen = true;
    app->window_drag_active = true;
    bongo_cat_window_drag_bounds_refresh(app);
    display_count = 1;
    SDL_Event removed = {.type = SDL_EVENT_DISPLAY_REMOVED};
    bongo_cat_window_display_event(app, &removed);
    CHECK(app->drag_display_count == 1 && display_queries == 2);
    bongo_cat_window_drag_to(app, 2200, 100);
    CHECK(window_x == 1600 && display_queries == 2);
    bongo_cat_window_drag_bounds_clear(app);

    reset(app);
    app->settings.window.keep_in_screen = true;
    window_x = 2200;
    SDL_Event event = {.type = SDL_EVENT_DISPLAY_REMOVED};
    bongo_cat_window_display_event(app, &event);
    display_count = 1;
    uint64_t due = app->display_recovery_due_ns;
    bongo_cat_window_update_display_recovery(app, due - 1);
    CHECK(window_x == 2200);
    bongo_cat_window_update_display_recovery(app, due);
    CHECK(window_x == 1600 && app->display_recovery_due_ns == 0);
    calls = position_calls;
    bongo_cat_window_update_display_recovery(app, due + 1);
    CHECK(position_calls == calls);

    reset(app);
    display_count = 1; window_x = -80;
    app->display_recovery_due_ns = 1000;
    bongo_cat_window_update_display_recovery(app, 1000);
    CHECK(window_x == -80);
    window_x = 2200; app->display_recovery_due_ns = 1000;
    bongo_cat_window_update_display_recovery(app, 1000);
    CHECK(window_x == 1600);
}

int main(void) {
    static BongoCatApp app;
    test_coverage();
    test_drag(&app);
    test_snap(&app);
    test_clamp_and_recovery(&app);
    return bongo_cat_test_failures ? 1 : 0;
}
