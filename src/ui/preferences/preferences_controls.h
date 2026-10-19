#ifndef BONGO_CAT_PREFERENCES_CONTROLS_H
#define BONGO_CAT_PREFERENCES_CONTROLS_H

#include <stdbool.h>
#include "bongo_cat/config.h"
#include "nuklear_config.h"

/* Stable, unique ids retain numeric drafts; valid edits update value immediately.
 * Integer fields accept digits and an optional minus; floats also accept a dot.
 * Values are clamped to the supplied range. Clicking away discards incomplete text.
 */
bool bongo_cat_pref_control_float(struct nk_context *context, const char *id,
    float minimum, float *value, float maximum, float step,
    float default_value);
bool bongo_cat_pref_control_int(struct nk_context *context, const char *id,
    int minimum, int *value, int maximum, int step, int default_value);
/* Same stepper placed in an explicit rectangle, for overlay dialogs that draw
   on the canvas instead of laying out widgets with nk_layout. */
bool bongo_cat_pref_control_int_rect(struct nk_context *context, const char *id,
    int minimum, int *value, int maximum, int step, int default_value,
    struct nk_rect bounds);
/* Slider with a value box on the right. The box is editable: click it to
 * type a number, double-click to restore the default. #suffix is drawn
 * inside the box while not editing (for example "%"). #edit_maximum lets
 * typed values exceed the slider range (dragging still clamps to
 * #maximum); pass 0 to use #maximum. */
bool bongo_cat_pref_control_slider(struct nk_context *context, const char *id,
    float minimum, float *value, float maximum, float step,
    float default_value, const char *suffix, float edit_maximum);
/* Same slider in an explicit rectangle, for overlay rows. */
bool bongo_cat_pref_control_slider_rect(struct nk_context *context, const char *id,
    float minimum, float *value, float maximum, float step,
    float default_value, const char *suffix, float edit_maximum,
    struct nk_rect bounds, bool available, float opacity);
bool bongo_cat_pref_control_toggle(struct nk_context *context,
    const char *id, bool *value);
bool bongo_cat_pref_control_toggle_available(struct nk_context *context,
    const char *id, bool *value, bool available);
/* Draws the toggle inside an explicit cell; used by overlay dialogs. */
bool bongo_cat_pref_control_toggle_rect(struct nk_context *context,
    const char *id, bool *value, struct nk_rect cell, bool available);
/* Keep HSV per picker so black and gray retain the selected hue. */
typedef struct BongoCatPrefColorState {
    float hue, saturation, value;
    uint32_t rgb;
    bool initialized, open;
} BongoCatPrefColorState;
/* Current-color button opens a shared HSV popup; updates while dragging. */
bool bongo_cat_pref_color_picker(struct nk_context *context,
    BongoCatPrefColorState *state, uint32_t *rgb);
int bongo_cat_pref_control_combo(struct nk_context *context, const char *id,
    const char *const *items, int count, int selected, bool fill_width);
bool bongo_cat_pref_controls_animating(struct nk_context *context);
void bongo_cat_pref_controls_reset(struct nk_context *context);

#endif
