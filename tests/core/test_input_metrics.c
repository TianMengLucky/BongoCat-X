#include "test.h"
#include "bongo_cat/input.h"
#include <math.h>
#include <string.h>

int bongo_cat_test_failures;

int main(void) {
    BongoCatInputState state = {0};
    bongo_cat_input_init(&state);
    BongoCatInputEvent key = {.kind = BONGO_CAT_INPUT_KEY_DOWN};
    memcpy(key.name, "KeyA", sizeof("KeyA"));
    bongo_cat_input_push(&state, &key);
    BongoCatStatisticsMetrics metrics;
    bongo_cat_input_metrics_take(&state, &metrics);
    CHECK(metrics.keys == 0);
    bongo_cat_input_metrics_enable(&state, true);
    /* Include attempts rejected by the pet's bounded queue. */
    for (unsigned i = 0; i < BONGO_CAT_INPUT_QUEUE_CAP + 20; ++i)
        bongo_cat_input_push(&state, &key);
    key.kind = BONGO_CAT_INPUT_KEY_UP;
    bongo_cat_input_push(&state, &key);
    BongoCatInputEvent click = {.kind = BONGO_CAT_INPUT_MOUSE_DOWN};
    memcpy(click.name, "Right", sizeof("Right"));
    bongo_cat_input_push(&state, &click);
    bongo_cat_input_mouse(&state, 0, 0);
    bongo_cat_input_mouse(&state, 3, 4);
    bongo_cat_input_mouse(&state, 0, 0);
    bongo_cat_input_metrics_scroll(&state, -0.5, 2);
    bongo_cat_input_metrics_scroll(&state, 0, -3);
    bongo_cat_input_metrics_scroll(&state, NAN, 1);
    bongo_cat_input_metrics_enable(&state, false);
    bongo_cat_input_push(&state, &click);
    bongo_cat_input_metrics_take(&state, &metrics);
    CHECK(metrics.keys == BONGO_CAT_INPUT_QUEUE_CAP + 20);
    CHECK(metrics.buttons[1] == 1);
    CHECK(metrics.movement == 10);
    CHECK(metrics.scroll == 5.5);
    bongo_cat_input_metrics_take(&state, &metrics);
    CHECK(metrics.keys == 0 && metrics.scroll == 0);
    bongo_cat_input_metrics_enable(&state, true);
    bongo_cat_input_mouse(&state, 1000, 1000);
    bongo_cat_input_metrics_motion(&state, 3, 4);
    bongo_cat_input_metrics_take(&state, &metrics);
    CHECK(metrics.movement == 5);
    bongo_cat_input_destroy(&state);
    return bongo_cat_test_failures ? 1 : 0;
}
