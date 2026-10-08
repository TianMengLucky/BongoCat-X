#include "mver_render.h"
#include "bongo_cat/json.h"
#include <math.h>
#include <limits.h>

static double number(BongoJsonValue *value, double fallback) {
    double result = bongo_json_is_num(value) ? bongo_json_get_num(value) : fallback;
    return isfinite(result) ? result : fallback;
}
static int coordinate(BongoJsonValue *value, int fallback) {
    double result = number(value, fallback);
    return result >= INT_MIN && result <= INT_MAX ? (int)result : fallback;
}
void bongo_cat_mver_render_options(BongoJsonValue *config,
    BongoCatModelRuntimeRenderOptions *options) {
    BongoJsonValue *decoration = bongo_json_obj_get(config, "decoration");
    BongoJsonValue *workarea = bongo_json_obj_get(config, "workarea");
    BongoJsonValue *window = bongo_json_obj_get(decoration, "window_size");
    BongoJsonValue *offset = bongo_json_obj_get(decoration, "l2d_offset");
    BongoJsonValue *top_left = bongo_json_obj_get(workarea, "top_left");
    BongoJsonValue *bottom_right = bongo_json_obj_get(workarea, "right_bottom");
    *options = (BongoCatModelRuntimeRenderOptions){
        .mver_projection = true,
        .auto_frame = bongo_json_get_bool(bongo_json_obj_get(decoration, "l2d_auto_frame")),
        .source_mirror = bongo_json_get_bool(bongo_json_obj_get(decoration, "l2d_horizontal_flip")),
        .pointer_left_handed = bongo_json_get_bool(bongo_json_obj_get(decoration, "leftHanded")),
        .mouse_force_move = bongo_json_get_bool(bongo_json_obj_get(decoration, "mouse_force_move")),
        .mouse_speed = (float)number(bongo_json_obj_get(decoration, "mouse_speed"), 1.0),
        .projection_scale = (float)number(bongo_json_obj_get(decoration, "l2d_correct"), 1.1),
        .offset_x = (float)number(bongo_json_arr_get(offset, 0), 0.0),
        .offset_y = (float)number(bongo_json_arr_get(offset, 1), 0.0),
        .reference_width = coordinate(bongo_json_arr_get(window, 0), 612),
        .reference_height = coordinate(bongo_json_arr_get(window, 1), 352),
        .custom_pointer_bounds = bongo_json_get_bool(bongo_json_obj_get(workarea, "workarea")),
        .pointer_left = coordinate(bongo_json_arr_get(top_left, 0), 0),
        .pointer_top = coordinate(bongo_json_arr_get(top_left, 1), 0),
        .pointer_right = coordinate(bongo_json_arr_get(bottom_right, 0), 0),
        .pointer_bottom = coordinate(bongo_json_arr_get(bottom_right, 1), 0)};
    if (options->projection_scale <= 0.0f || options->projection_scale > 100.0f)
        options->projection_scale = 1.1f;
    if (options->reference_width <= 0 || options->reference_height <= 0) {
        options->reference_width = 612;
        options->reference_height = 352;
    }
    if (options->pointer_right <= options->pointer_left ||
        options->pointer_bottom <= options->pointer_top)
        options->custom_pointer_bounds = false;
}
bool bongo_cat_mver_render_read(const char *path,
    BongoCatModelRuntimeRenderOptions *options) {
    BongoJsonDoc *doc = bongo_cat_json_read_file(path,
        BONGO_JSON_READ_JSON5 | BONGO_JSON_READ_ALLOW_INVALID_UNICODE, NULL);
    bool ok = doc && bongo_json_is_obj(bongo_json_doc_get_root(doc));
    if (ok) bongo_cat_mver_render_options(bongo_json_doc_get_root(doc), options);
    bongo_json_doc_free(doc);
    return ok;
}
