#include "model_import_tauri_internal.h"
#include "bongo_cat/safe_import.h"
int bongo_cat_tauri_key_code(const char *filename) {
    return bongo_safe_tauri_key_code(filename);
}
