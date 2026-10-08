#include "live2d_audit_scenario.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void bongo_cat_model_runtime_audit_input(BongoCatApp *app, BongoCatInputKind kind,
    const char *name, float value) {
    BongoCatInputEvent event = {.kind = kind, .value = value};
    snprintf(event.name, sizeof(event.name), "%s", name);
    bongo_cat_app_shortcuts(app, &event);
    bongo_cat_app_apply_input(app, &event);
}

bool bongo_cat_model_runtime_audit_motion(BongoCatApp *app, const char *scenario) {
    if (strcmp(scenario, "motion-0") == 0)
        return bongo_cat_model_runtime_start_motion(app->model_runtime, "CAT_motion", 0);
    if (strcmp(scenario, "motion-1") == 0)
        return bongo_cat_model_runtime_start_motion(app->model_runtime, "CAT_motion", 1);
    if (strncmp(scenario, "expression-", 11) == 0)
        return bongo_cat_model_runtime_set_expression(app->model_runtime,
            atoi(scenario + 11));
    return true;
}

bool bongo_cat_model_runtime_audit_parameter_override(const char *scenario) {
    return strncmp(scenario, "key-", 4) == 0 ||
        strncmp(scenario, "keys-", 5) == 0 ||
        strncmp(scenario, "gamepad-", 8) == 0 ||
        strcmp(scenario, "mouse-left") == 0 ||
        strcmp(scenario, "mouse-right") == 0;
}
