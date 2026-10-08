#ifndef BONGO_CAT_LIVE2D_POINTER_AUDIT_H
#define BONGO_CAT_LIVE2D_POINTER_AUDIT_H

#include "runtime.h"

typedef struct BongoCatPointerAudit {
    bool ran;
    bool has_mouse;
    float angle_x[4];
    float angle_y[4];
    float mouse_x[4];
    float mouse_y[4];
    float maximum_step;
} BongoCatPointerAudit;

extern BongoCatPointerAudit bongo_cat_pointer_audit;

bool bongo_cat_model_runtime_pointer_audit_run(BongoCatApp *app, bool mirror);
bool bongo_cat_model_runtime_pointer_reverse_audit_run(BongoCatApp *app);

#endif
