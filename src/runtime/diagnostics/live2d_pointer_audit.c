#include "live2d_pointer_audit.h"

BongoCatPointerAudit bongo_cat_pointer_audit;

static bool value(BongoCatApp *app, const char *id, float *output) {
    BongoCatParameterRange range;
    if (!bongo_cat_model_runtime_parameter(app->model_runtime, id, &range)) return false;
    if (output) *output = range.value;
    return true;
}

static bool test_center(BongoCatApp *app, SDL_Rect *bounds,
    double *center_x, double *center_y) {
    int window_x, window_y, width, height;
    if (!app || !app->window || !bounds || !center_x || !center_y ||
        !SDL_GetWindowPosition(app->window, &window_x, &window_y) ||
        !SDL_GetWindowSize(app->window, &width, &height)) return false;
    *center_x = window_x + width * 0.5;
    *center_y = window_y + height * 0.5;
    SDL_Point point = {(int)*center_x, (int)*center_y};
    SDL_DisplayID display = SDL_GetDisplayForPoint(&point);
    return display && SDL_GetDisplayBounds(display, bounds) &&
        bounds->w > 0 && bounds->h > 0;
}

bool bongo_cat_model_runtime_pointer_audit_run(BongoCatApp *app, bool mirror) {
    SDL_Rect bounds;
    double center_x, center_y;
    if (!test_center(app, &bounds, &center_x, &center_y)) return false;
    app->settings.model.mouse_mirror = mirror;
    app->left_mouse_down = false;
    bongo_cat_app_apply_mouse_position(app, center_x + bounds.w * 0.4,
        center_y - bounds.h * 0.4, 1.0f / 60.0f);
    for (int frame = 0; frame < 90; ++frame)
        bongo_cat_app_step_live2d(app, 1.0f / 60.0f);
    return true;
}

bool bongo_cat_model_runtime_pointer_reverse_audit_run(BongoCatApp *app) {
    SDL_Rect bounds;
    double center_x, center_y;
    if (!test_center(app, &bounds, &center_x, &center_y)) return false;
    bongo_cat_pointer_audit = (BongoCatPointerAudit){.ran = true};
    app->settings.model.mouse_mirror = false;
    app->left_mouse_down = false;
    const float ratios[4][2] = {
        {-0.4f, -0.4f}, {0.4f, -0.4f}, {-0.4f, 0.4f}, {0.4f, 0.4f}};
    float previous_x = 0.0f;
    bool previous_ready = value(app, "ParamAngleX", &previous_x);
    for (int corner = 0; corner < 4; ++corner) {
        bongo_cat_app_apply_mouse_position(app,
            center_x + bounds.w * ratios[corner][0],
            center_y + bounds.h * ratios[corner][1], 1.0f / 60.0f);
        for (int frame = 0; frame < 90; ++frame) {
            bongo_cat_app_step_live2d(app, 1.0f / 60.0f);
            float current_x = previous_x;
            if (!value(app, "ParamAngleX", &current_x)) return false;
            if (previous_ready) {
                float step = SDL_fabsf(current_x - previous_x);
                if (step > bongo_cat_pointer_audit.maximum_step)
                    bongo_cat_pointer_audit.maximum_step = step;
            }
            previous_x = current_x;
            previous_ready = true;
        }
        if (!value(app, "ParamAngleX",
                &bongo_cat_pointer_audit.angle_x[corner]) ||
            !value(app, "ParamAngleY",
                &bongo_cat_pointer_audit.angle_y[corner])) return false;
        bool mouse_x = value(app, "ParamMouseX",
            &bongo_cat_pointer_audit.mouse_x[corner]);
        bool mouse_y = value(app, "ParamMouseY",
            &bongo_cat_pointer_audit.mouse_y[corner]);
        if (corner == 0)
            bongo_cat_pointer_audit.has_mouse = mouse_x && mouse_y;
        else if (bongo_cat_pointer_audit.has_mouse != (mouse_x && mouse_y))
            return false;
    }
    return true;
}
