#ifndef BONGO_CAT_INPUT_H
#define BONGO_CAT_INPUT_H

#include "bongo_cat/common.h"
#ifndef __cplusplus
#include <stdatomic.h>
#endif
#include "bongo_cat/statistics_plugin.h"

#define BONGO_CAT_INPUT_QUEUE_CAP 256u
#define BONGO_CAT_INPUT_RECOVERY_CAP 65u
#define BONGO_CAT_INPUT_KEY_STATE_CAP 256u
/* Ignore small analog stick offsets around the resting position. */
#define BONGO_CAT_GAMEPAD_STICK_DEADZONE 0.10f

typedef enum BongoCatInputKind {
    BONGO_CAT_INPUT_NONE,
    BONGO_CAT_INPUT_KEY_DOWN,
    BONGO_CAT_INPUT_KEY_UP,
    BONGO_CAT_INPUT_MOUSE_DOWN,
    BONGO_CAT_INPUT_MOUSE_UP,
    BONGO_CAT_INPUT_MOUSE_MOVE,
    BONGO_CAT_INPUT_GAMEPAD_BUTTON,
    BONGO_CAT_INPUT_GAMEPAD_AXIS
} BongoCatInputKind;

typedef struct BongoCatInputEvent {
    BongoCatInputKind kind;
    uint64_t timestamp_ms;
    char name[BONGO_CAT_ID_CAP];
    float value;
    double x;
    double y;
    uint64_t sequence;
} BongoCatInputEvent;

typedef struct BongoCatScheduledRelease {
    uint64_t deadline_ms;
    char name[BONGO_CAT_ID_CAP];
} BongoCatScheduledRelease;

/* Zero-initialize before first init. Rust owns the queues and synchronization.
   Destroy only after every platform producer/consumer has stopped. */
typedef struct BongoCatInputState { void *core; } BongoCatInputState;

#ifdef __cplusplus
extern "C" {
#endif

void bongo_cat_input_destroy(BongoCatInputState *state);
uint64_t bongo_cat_input_dropped(const BongoCatInputState *state);
bool bongo_cat_input_metrics_enabled(const BongoCatInputState *state);
void bongo_cat_input_init(BongoCatInputState *state);
bool bongo_cat_input_edge(bool states[BONGO_CAT_INPUT_KEY_STATE_CAP],
    unsigned code, bool down);
bool bongo_cat_input_push(BongoCatInputState *state, const BongoCatInputEvent *event);
bool bongo_cat_input_pop(BongoCatInputState *state, BongoCatInputEvent *event);
bool bongo_cat_input_mouse(BongoCatInputState *state, double x, double y);
bool bongo_cat_input_take_mouse(BongoCatInputState *state, double *x, double *y);
bool bongo_cat_input_control_down(const BongoCatInputState *state);
bool bongo_cat_input_shift_down(const BongoCatInputState *state);
void bongo_cat_input_schedule_release(BongoCatInputState *state,
    const BongoCatInputEvent *event, uint64_t delay_ms);
bool bongo_cat_input_take_scheduled_release(BongoCatInputState *state,
    uint64_t now_ms, BongoCatInputEvent *event);

void bongo_cat_input_metrics_enable(BongoCatInputState *state, bool enabled);
void bongo_cat_input_metrics_event(BongoCatInputState *state, const BongoCatInputEvent *event);
void bongo_cat_input_metrics_pointer(BongoCatInputState *state, double x, double y);
void bongo_cat_input_metrics_motion(BongoCatInputState *state, double x, double y);
void bongo_cat_input_metrics_scroll(BongoCatInputState *state, double x, double y);
void bongo_cat_input_metrics_take(BongoCatInputState *state, BongoCatStatisticsMetrics *metrics);

#ifdef __cplusplus
}
#endif

#endif
