#include "model_import_tauri_internal.h"
#include "bongo_cat/safe_import.h"
void bongo_cat_tauri_calibration_defaults(TauriMverCalibration *c) {
    *c = (TauriMverCalibration){.l2d_correct = 1.1, .window_width = 612, .window_height = 352};
}
bool bongo_cat_tauri_read_calibration(const BongoCatImportCandidate *candidate, TauriMverCalibration *c) {
    if (!candidate || !c) return false;
    bongo_cat_tauri_legacy_calibration(candidate, c);
    char path[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_tauri_find_package_file(candidate, BONGO_CAT_TAURI_SOURCE_FILE, path, sizeof(path))) return true;
    BongoSafeCalibration value = {c->l2d_correct, c->l2d_offset_x, c->l2d_offset_y,
        c->window_width, c->window_height, c->mirror, c->auto_frame};
    if (bongo_safe_tauri_calibration(path, bongo_cat_mode_name(candidate->mode), &value, sizeof(value))) {
        c->l2d_correct = value.scale; c->l2d_offset_x = value.x; c->l2d_offset_y = value.y;
        c->window_width = value.width; c->window_height = value.height;
        c->mirror = value.mirror; c->auto_frame = value.auto_frame;
    }
    return true;
}
