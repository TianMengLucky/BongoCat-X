#include "model_import_tauri_internal.h"

#include "bongo_cat/json.h"

#include "bongo_cat/json_dom.h"

static bool add_matrix(BongoJsonMutDoc *document, BongoJsonMutValue *mode,
    const char *name, const TauriKeyFiles *files) {
    BongoJsonMutValue *matrix = bongo_json_mut_obj_add_arr(document, mode, name);
    if (!matrix) return false;
    for (size_t i = 0; i < files->count; ++i) {
        BongoJsonMutValue *row = bongo_json_mut_arr_add_arr(document, matrix);
        if (!row || !bongo_json_mut_arr_add_int(document, row,
                files->values[i].code)) return false;
    }
    return true;
}

bool bongo_cat_tauri_write_config(const char *path, BongoCatModelMode mode,
    const TauriKeyFiles *left, const TauriKeyFiles *right,
    const TauriMverCalibration *calibration, BongoCatError *error) {
    if (!path || !left || !right || !calibration) return false;
    BongoJsonMutDoc *document = bongo_json_mut_doc_new(NULL);
    BongoJsonMutValue *root = document ? bongo_json_mut_obj(document) : NULL;
    if (document) bongo_json_mut_doc_set_root(document, root);
    BongoJsonMutValue *decoration = root ? bongo_json_mut_obj_add_obj(document, root,
        "decoration") : NULL;
    BongoJsonMutValue *mode_object = root ? bongo_json_mut_obj_add_obj(document, root,
        bongo_cat_mode_name(mode)) : NULL;
    BongoJsonMutValue *window_size = decoration ? bongo_json_mut_obj_add_arr(document,
        decoration, "window_size") : NULL;
    BongoJsonMutValue *offset = decoration ? bongo_json_mut_obj_add_arr(document,
        decoration, "l2d_offset") : NULL;
    bool ok = root && decoration && mode_object && window_size && offset &&
        bongo_json_mut_arr_add_int(document, window_size,
            calibration->window_width) &&
        bongo_json_mut_arr_add_int(document, window_size,
            calibration->window_height) &&
        bongo_json_mut_obj_add_real(document, decoration, "l2d_correct",
            calibration->l2d_correct) &&
        bongo_json_mut_arr_add_real(document, offset, calibration->l2d_offset_x) &&
        bongo_json_mut_arr_add_real(document, offset, calibration->l2d_offset_y) &&
        bongo_json_mut_obj_add_bool(document, decoration, "l2d_horizontal_flip",
            calibration->mirror) &&
        bongo_json_mut_obj_add_bool(document, decoration, "l2d_auto_frame",
            calibration->auto_frame) &&
        bongo_json_mut_obj_add_int(document, root, "mode",
            mode == BONGO_CAT_MODE_STANDARD ? 1 :
            mode == BONGO_CAT_MODE_KEYBOARD ? 2 : 3) &&
        bongo_json_mut_obj_add_bool(document, mode_object, "l2d", true);
    if (ok && mode == BONGO_CAT_MODE_STANDARD)
        ok = add_matrix(document, mode_object, "hand", left) &&
            bongo_json_mut_obj_add_bool(document, mode_object, "mouse", false) &&
            bongo_json_mut_obj_add_arr(document, mode_object, "keyboard");
    if (ok && mode != BONGO_CAT_MODE_STANDARD)
        ok = add_matrix(document, mode_object, "lefthand", left) &&
            add_matrix(document, mode_object, "righthand", right) &&
            bongo_json_mut_obj_add_arr(document, mode_object, "keyboard");
    if (ok && mode == BONGO_CAT_MODE_GAMEPAD)
        ok = bongo_json_mut_obj_add_int(document, mode_object, "input_mode", 1);
    if (ok) ok = bongo_json_mut_obj_add_arr(document, mode_object, "face") &&
        bongo_json_mut_obj_add_arr(document, mode_object, "sounds") &&
        bongo_json_mut_obj_add_arr(document, mode_object, "l2d_expression") &&
        bongo_json_mut_obj_add_arr(document, mode_object, "l2d_motion") &&
        bongo_json_mut_obj_add_arr(document, mode_object, "l2d_motion_lockhand");
    if (ok) ok = bongo_cat_json_write_file(path, document,
        BONGO_JSON_WRITE_PRETTY, NULL);
    bongo_json_mut_doc_free(document);
    if (!ok && error) bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
        "Cannot write normalized Mver configuration: %s", path);
    return ok;
}
