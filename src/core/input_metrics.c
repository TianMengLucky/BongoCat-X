#include "bongo_cat/input.h"
#include <math.h>
#include <string.h>
static bool collecting(BongoCatInputState *state) {
    return state && atomic_load_explicit(&state->metrics_enabled, memory_order_relaxed);
}
static void add_units(atomic_uint_fast64_t *counter, double value) {
    /* Fixed-point atomics avoid sharing a lock with the consumer. */
    if (isfinite(value) && value > 0.0 && value < (double)UINT64_MAX / 1024.0)
        atomic_fetch_add_explicit(counter, (uint64_t)(value * 1024.0 + 0.5), memory_order_relaxed);
}
void bongo_cat_input_metrics_enable(BongoCatInputState *state, bool enabled) {
    if (!state) return;
    if (enabled) atomic_store_explicit(&state->metrics_reset, true, memory_order_relaxed);
    atomic_store_explicit(&state->metrics_enabled, enabled, memory_order_release);
}
void bongo_cat_input_metrics_event(BongoCatInputState *state, const BongoCatInputEvent *event) {
    if (!collecting(state) || !event) return;
    if (event->kind == BONGO_CAT_INPUT_KEY_DOWN)
        atomic_fetch_add_explicit(&state->metrics_keys, 1, memory_order_relaxed);
    else if (event->kind == BONGO_CAT_INPUT_MOUSE_DOWN) {
        static const char *names[] = {"Left", "Right", "Middle", "Back", "Forward"};
        for (unsigned i = 0; i < 5; ++i)
            if (!strcmp(event->name, names[i])) {
                atomic_fetch_add_explicit(&state->metrics_buttons[i], 1, memory_order_relaxed); break;
            }
    }
}
void bongo_cat_input_metrics_pointer(BongoCatInputState *state, double x, double y) {
    if (!state || !isfinite(x) || !isfinite(y)) return;
    /* Pointer producers try once; neither the pet nor the worker waits here. */
    if (atomic_exchange_explicit(&state->metrics_busy, true, memory_order_acquire)) return;
    if (atomic_exchange_explicit(&state->metrics_reset, false, memory_order_relaxed)) state->metrics_pointer_known = false;
    if (collecting(state) && state->metrics_pointer_known)
        add_units(&state->metrics_movement, hypot(x - state->metrics_x, y - state->metrics_y));
    state->metrics_x = x; state->metrics_y = y;
    state->metrics_pointer_known = collecting(state);
    atomic_store_explicit(&state->metrics_busy, false, memory_order_release);
}
void bongo_cat_input_metrics_motion(BongoCatInputState *state, double x, double y) {
    if (collecting(state)) add_units(&state->metrics_movement, hypot(x, y));
}
void bongo_cat_input_metrics_scroll(BongoCatInputState *state, double x, double y) {
    if (collecting(state)) add_units(&state->metrics_scroll, fabs(x) + fabs(y));
}
void bongo_cat_input_metrics_take(BongoCatInputState *state, BongoCatStatisticsMetrics *metrics) {
    if (!state || !metrics) return;
    metrics->keys = atomic_exchange_explicit(&state->metrics_keys, 0, memory_order_relaxed);
    for (unsigned i = 0; i < 5; ++i)
        metrics->buttons[i] = atomic_exchange_explicit(&state->metrics_buttons[i], 0, memory_order_relaxed);
    metrics->movement = (double)atomic_exchange_explicit(&state->metrics_movement, 0, memory_order_relaxed) / 1024.0;
    metrics->scroll = (double)atomic_exchange_explicit(&state->metrics_scroll, 0, memory_order_relaxed) / 1024.0;
}
