#include "bongo_cat/input.h"
/* Keep C layout validation at the boundary; all mutable state lives in Rust. */
extern void *bongo_safe_input_new(size_t event_size, size_t metrics_size);
void bongo_cat_input_init(BongoCatInputState *state) {
    if (!state) return;
    bongo_cat_input_destroy(state);
    state->core = bongo_safe_input_new(sizeof(BongoCatInputEvent),
        sizeof(BongoCatStatisticsMetrics));
}
