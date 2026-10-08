#ifndef BONGO_CAT_LIVE2D_AUDIT_SCENARIO_H
#define BONGO_CAT_LIVE2D_AUDIT_SCENARIO_H

#include "runtime.h"

void bongo_cat_model_runtime_audit_input(BongoCatApp *app, BongoCatInputKind kind,
    const char *name, float value);
bool bongo_cat_model_runtime_audit_motion(BongoCatApp *app, const char *scenario);
bool bongo_cat_model_runtime_audit_parameter_override(const char *scenario);

#endif
