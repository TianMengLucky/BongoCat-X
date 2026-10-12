#include "preferences_state.h"
#include "preferences_widgets.h"
#include "ui_icons.h"
#include "runtime.h"
#include "bongo_cat/i18n.h"
static const char *tr(BongoCatApp *app, const char *key, const char *fallback) {
    return bongo_cat_i18n_get(app->i18n, key, fallback);
}
void bongo_cat_preferences_mouse_range_row(BongoCatApp *app, struct nk_context *context) {
    BongoCatModelPreferences *model = &app->settings.model;
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_MOUSE_MIRROR);
    bool changed = bongo_cat_pref_toggle(context, "custom-mouse-range", tr(app,
        "pages.preference.cat.labels.customMouseRange", "Custom Mouse Follow Range"), tr(app,
        "pages.preference.cat.hints.customMouseRange",
        "100% preserves the original range; increase to extend the drawn Mver hand and mouse. Live2D parameters remain within the model's authored limits. Gaze and window position are unchanged."),
        &model->custom_mouse_range);
    if (model->custom_mouse_range) {
        changed |= bongo_cat_pref_slider(context, "mouse-range-x", tr(app,
            "pages.preference.cat.labels.mouseRangeX", "Horizontal range (%)"), "",
            25.0f, &model->mouse_range_x_percent, 400.0f, 5.0f, 100.0f);
        changed |= bongo_cat_pref_slider(context, "mouse-range-y", tr(app,
            "pages.preference.cat.labels.mouseRangeY", "Vertical range (%)"), "",
            25.0f, &model->mouse_range_y_percent, 400.0f, 5.0f, 100.0f);
    }
    if (changed) bongo_cat_app_reset_pointer_tracking(app);
}
