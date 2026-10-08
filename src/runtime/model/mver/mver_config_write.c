#include "mver_config_text.h"
#include "bongo_cat/file.h"
#include "bongo_cat/json_dom.h"
#include "bongo_cat/safe_ffi.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool bongo_cat_mver_config_write_row(const char *path, const char *mode,
    const char *field, int index, const char *row, BongoCatError *error) {
    if (!path || !mode || !field || !row) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_ARGUMENT, "Invalid Mver shortcut row");
        return false;
    }
    size_t length = 0;
    char *original = mver_text_read(path, &length);
    char *edited = original ? bongo_safe_mver_edit(original, length, mode, field, index, row) : NULL;
    free(original);
    if (!edited) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT, "Cannot parse or edit Mver configuration: %s", path);
        return false;
    }
    char temporary[BONGO_CAT_PATH_CAP];
    int n = snprintf(temporary, sizeof(temporary), "%s.mver-%llu.tmp", path,
        (unsigned long long)SDL_GetTicksNS());
    bool ok = n > 0 && (size_t)n < sizeof(temporary);
    FILE *file = ok ? bongo_cat_file_open(temporary, "wb") : NULL;
    length = strlen(edited);
    ok = file && fwrite(edited, 1, length, file) == length;
    if (file && fclose(file) != 0) ok = false;
    bongo_json_free_text(edited);
    if (ok) ok = bongo_cat_file_replace(temporary, path, true);
    if (!ok && n > 0 && (size_t)n < sizeof(temporary)) bongo_cat_file_remove(temporary);
    if (!ok) bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Cannot save Mver configuration: %s", path);
    return ok;
}
