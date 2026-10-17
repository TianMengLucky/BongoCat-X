#include "model_import_tauri_internal.h"
#include "bongo_cat/safe_import.h"
bool bongo_cat_tauri_write_config(const char *path, BongoCatModelMode mode,
    const TauriKeyFiles *left, const TauriKeyFiles *right,
    const TauriMverCalibration *c, BongoCatError *error) {
    if (!path || !left || !right || !c || left->count > TAURI_KEY_CAP || right->count > TAURI_KEY_CAP) return false;
    int left_codes[TAURI_KEY_CAP] = {0}, right_codes[TAURI_KEY_CAP] = {0};
    for (size_t i = 0; i < left->count; ++i) left_codes[i] = left->values[i].code;
    for (size_t i = 0; i < right->count; ++i) right_codes[i] = right->values[i].code;
    BongoSafeCalibration value = {c->l2d_correct, c->l2d_offset_x, c->l2d_offset_y,
        c->window_width, c->window_height, c->mirror, c->auto_frame};
    bool ok = bongo_safe_tauri_config(path, bongo_cat_mode_name(mode), left_codes,
        left->count, right_codes, right->count, &value, sizeof(value));
    if (!ok) bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Cannot write normalized Mver configuration: %s", path);
    return ok;
}
