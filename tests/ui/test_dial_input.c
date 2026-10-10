/* Radial menu input contract: hovering a parent and scrolling selects a child,
   left click confirms it without dismissing the menu, right click rolls the
   pending selection back and keeps the menu open. */
#include "dial_internal.h"
#include "test.h"
#include <string.h>

int bongo_cat_test_failures = 0;

static BongoCatMenuAction last_preview;
static BongoCatMenuAction last_restore;
static int preview_calls, restore_calls;

/* dial_window.c owns the real preview helper (it also juggles GL contexts);
   the input tests only need to observe what the dial asks the host to do. */
void dial_preview(Dial *d, BongoCatMenuAction action) {
    (void)d;
    last_preview = action;
    preview_calls++;
}

static void record_restore(void *userdata, BongoCatMenuAction action) {
    (void)userdata;
    last_restore = action;
    restore_calls++;
}

static Dial dial;
static BongoCatMenuLabels labels;
static char motion_names[24][BONGO_CAT_MENU_LABEL_CAP];
static bool motion_checked[24];

static const int SIZE_ROOT = 4;      /* Window Size: 16 children */
static const int MOTION_ROOT = 6;    /* Motions: spans two pages */
static const int MIRROR_ROOT = 1;    /* Leaf: executes and dismisses */
static const float RING_ROOT = 130;  /* Parent sector mid radius */
static const float RING_CHILD = 230; /* Child ring mid radius */

static void setup(void) {
    memset(&dial, 0, sizeof(dial));
    memset(&labels, 0, sizeof(labels));
    memset(motion_names, 0, sizeof(motion_names));
    memset(motion_checked, 0, sizeof(motion_checked));
    labels.preferences = "Preferences";
    labels.mirror = "Mirror Mode";
    labels.vertical_flip = "Hang Upside Down";
    labels.always_on_top = "Always on top";
    labels.window_size = "Window Size";
    labels.opacity = "Opacity";
    labels.motion = "Motions";
    labels.expression = "Expressions";
    labels.audio = "Audio";
    labels.model = "Model";
    labels.add_model = "Add Model";
    labels.exit = "Exit";
    labels.remove_pet = "Close this desktop pet";
    labels.remove_pet_visible = true;
    labels.scale_percent = 100.0f;
    labels.opacity_percent = 100.0f;
    labels.motion_names = motion_names;
    labels.motion_checked = motion_checked;
    labels.motion_count = 20;
    labels.restore = record_restore;
    dial.labels = &labels;
    dial.width = dial.height = 608;
    dial.scale = 1.0f;
    dial.opening = 1.0f;
    dial.active = dial.child = dial.pressed = -1;
    dial_items(&dial);
    last_preview = last_restore = BONGO_CAT_MENU_NONE;
    preview_calls = restore_calls = 0;
}

static void to_screen(const Dial *d, float angle, float radius, float *x, float *y) {
    *x = radius * cosf(angle) + (float)d->width / 2;
    *y = radius * sinf(angle) + (float)d->height / 2;
}

static void hover_root(int index) {
    float x, y;
    to_screen(&dial, -DIAL_PI / 2 + index * 2 * DIAL_PI / dial.count,
        RING_ROOT, &x, &y);
    SDL_Event e = {0};
    e.type = SDL_EVENT_MOUSE_MOTION;
    e.motion.x = x; e.motion.y = y;
    dial_event(&dial, &e);
}

static void hover_child(int index) {
    float x, y;
    to_screen(&dial, dial_child_angle(&dial, index), RING_CHILD, &x, &y);
    SDL_Event e = {0};
    e.type = SDL_EVENT_MOUSE_MOTION;
    e.motion.x = x; e.motion.y = y;
    dial_event(&dial, &e);
}

static void press(Uint8 button, float x, float y) {
    SDL_Event e = {0};
    e.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    e.button.button = button; e.button.x = x; e.button.y = y;
    dial_event(&dial, &e);
    e.type = SDL_EVENT_MOUSE_BUTTON_UP;
    dial_event(&dial, &e);
}

static void click_root(int index, Uint8 button) {
    float x, y;
    to_screen(&dial, -DIAL_PI / 2 + index * 2 * DIAL_PI / dial.count,
        RING_ROOT, &x, &y);
    press(button, x, y);
}

static void click_far(Uint8 button) {
    press(button, (float)dial.width, (float)dial.height);
}

static void scroll(float y) {
    SDL_Event e = {0};
    e.type = SDL_EVENT_MOUSE_WHEEL;
    e.wheel.y = y;
    dial_event(&dial, &e);
}

static void select_then_scroll(int root, int notches) {
    hover_root(root);
    for (int i = 0; i < notches; ++i) scroll(-1.0f);
}

/* Scrolling over a parent walks its children one notch at a time. */
static void test_wheel_selects_children(void) {
    setup();
    hover_root(SIZE_ROOT);
    CHECK(dial.active == SIZE_ROOT);
    CHECK(dial.child == -1);
    scroll(-1.0f);
    CHECK(dial.child == 0);
    CHECK(last_preview == BONGO_CAT_MENU_SCALE_50);
    scroll(-1.0f);
    CHECK(dial.child == 1);
    CHECK(last_preview == BONGO_CAT_MENU_SCALE_60);
    scroll(1.0f);
    CHECK(dial.child == 0);
    CHECK(last_preview == BONGO_CAT_MENU_SCALE_50);
}

/* Scrolling up before anything is selected lands on the last child. */
static void test_wheel_wraps_backwards(void) {
    setup();
    hover_root(SIZE_ROOT);
    scroll(1.0f);
    CHECK(dial.child == 15);
    CHECK(last_preview == BONGO_CAT_MENU_SCALE_200);
}

/* A wheel-selected child survives the pointer drifting on the same parent,
   while focus follows the pointer so the other parents stay reachable. */
static void test_wheel_selection_persists(void) {
    setup();
    select_then_scroll(SIZE_ROOT, 2);
    CHECK(dial.child == 1);
    CHECK(dial.child_focus);
    hover_root(SIZE_ROOT);
    CHECK(dial.child == 1);
    CHECK(!dial.child_focus);
    /* Moving to another parent drops the pending child. */
    hover_root(MIRROR_ROOT);
    CHECK(dial.active == MIRROR_ROOT);
    CHECK(dial.child == -1);
    CHECK(!dial.child_focus);
}

/* Left click on the parent confirms the child in place and keeps the menu. */
static void test_left_click_confirms_parent(void) {
    setup();
    select_then_scroll(SIZE_ROOT, 2);
    click_root(SIZE_ROOT, SDL_BUTTON_LEFT);
    CHECK(restore_calls == 1);
    CHECK(last_restore == BONGO_CAT_MENU_SCALE_60);
    CHECK(!dial.done);
    CHECK(dial.child == 1);
}

/* Left click on a child confirms it the same way. */
static void test_left_click_confirms_child(void) {
    setup();
    select_then_scroll(SIZE_ROOT, 2);
    hover_child(1);
    CHECK(dial.child == 1);
    float x, y;
    to_screen(&dial, dial_child_angle(&dial, 1), RING_CHILD, &x, &y);
    press(SDL_BUTTON_LEFT, x, y);
    CHECK(restore_calls == 1);
    CHECK(last_restore == BONGO_CAT_MENU_SCALE_60);
    CHECK(!dial.done);
    CHECK(dial.child == 1);
}

/* Right click drops the pending child, rolls the preview back, stays open. */
static void test_right_click_cancels(void) {
    setup();
    select_then_scroll(SIZE_ROOT, 2);
    click_root(SIZE_ROOT, SDL_BUTTON_RIGHT);
    CHECK(!dial.done);
    CHECK(restore_calls == 0);
    CHECK(last_preview == BONGO_CAT_MENU_NONE);
    CHECK(dial.child == -1);
    CHECK(!dial.child_focus);
    CHECK(dial.active == SIZE_ROOT);
}

/* A parent with no pending child only opens its ring. */
static void test_left_click_opens_ring(void) {
    setup();
    hover_root(SIZE_ROOT);
    click_root(SIZE_ROOT, SDL_BUTTON_LEFT);
    CHECK(!dial.done);
    CHECK(dial.child == 0);
    CHECK(restore_calls == 0);
}

/* Leaf commands still execute and dismiss the menu. */
static void test_leaf_click_dismisses(void) {
    setup();
    hover_root(MIRROR_ROOT);
    click_root(MIRROR_ROOT, SDL_BUTTON_LEFT);
    CHECK(dial.done);
    CHECK(dial.result == BONGO_CAT_MENU_MIRROR);
}

/* Clicking away from the dial still dismisses it with either button. */
static void test_click_away_dismisses(void) {
    setup();
    select_then_scroll(SIZE_ROOT, 2);
    click_far(SDL_BUTTON_LEFT);
    CHECK(dial.done);
    setup();
    select_then_scroll(SIZE_ROOT, 2);
    click_far(SDL_BUTTON_RIGHT);
    CHECK(dial.done);
}

/* Children past the first page turn the page and keep walking. */
static void test_wheel_crosses_pages(void) {
    setup();
    hover_root(MOTION_ROOT);
    scroll(-1.0f);                       /* first notch selects the first child */
    CHECK(dial.page == 0);
    CHECK(dial.child == 0);
    for (int i = 0; i < 15; ++i) scroll(-1.0f);
    CHECK(dial.child == 15);
    scroll(-1.0f);                       /* past the edge: turn the page, walk on */
    CHECK(dial.page == 1);
    CHECK(dial.child == 0);
    CHECK(last_preview == BONGO_CAT_MENU_MOTION_FIRST + 16);
    scroll(1.0f);                        /* backwards across the page boundary */
    CHECK(dial.page == 0);
    CHECK(dial.child == 15);
}

/* Escape clears the pending child first, then closes on a second press. */
static void test_escape_clears_then_closes(void) {
    setup();
    select_then_scroll(SIZE_ROOT, 2);
    SDL_Event e = {0};
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.key = SDLK_ESCAPE;
    dial_event(&dial, &e);
    CHECK(!dial.done);
    CHECK(dial.child == -1);
    dial_event(&dial, &e);
    CHECK(dial.done);
}

int main(void) {
    test_wheel_selects_children();
    test_wheel_wraps_backwards();
    test_wheel_selection_persists();
    test_left_click_confirms_parent();
    test_left_click_confirms_child();
    test_right_click_cancels();
    test_left_click_opens_ring();
    test_leaf_click_dismisses();
    test_click_away_dismisses();
    test_wheel_crosses_pages();
    test_escape_clears_then_closes();
    if (bongo_cat_test_failures) {
        fprintf(stderr, "dial-input: %d check(s) failed\n", bongo_cat_test_failures);
        return 1;
    }
    printf("dial-input: all checks passed\n");
    return 0;
}
