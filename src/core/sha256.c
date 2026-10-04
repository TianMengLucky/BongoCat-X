#include "bongo_cat/file.h"
#include "bongo_cat/safe_ffi.h"
#include "bongo_cat/sha256.h"

#include <stdio.h>

void bongo_cat_sha256_bytes(const void *data, size_t size, char output[65]) {
    if (output) bongo_safe_sha256_bytes(data, size, output);
}

BongoCatResult bongo_cat_sha256_file(const char *path, char output[65], BongoCatError *error) {
    return bongo_cat_sha256_file_cancellable(path, output, NULL, NULL, error);
}

BongoCatResult bongo_cat_sha256_file_cancellable(const char *path, char output[65],
    BongoCatSha256Cancelled cancelled, void *userdata, BongoCatError *error) {
    if (!output) return BONGO_CAT_ERROR_ARGUMENT;
    output[0] = '\0';
    if (cancelled && cancelled(userdata)) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM, "File hash cancelled");
        return BONGO_CAT_ERROR_PLATFORM;
    }
    FILE *file = path ? bongo_cat_file_open(path, "rb") : NULL;
    if (!file) { bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Cannot open file"); return BONGO_CAT_ERROR_IO; }
    bongo_safe_sha256 *state = bongo_safe_sha256_new();
    if (!state) {
        fclose(file);
        bongo_cat_error_set(error, BONGO_CAT_ERROR_MEMORY, "Cannot allocate hash state");
        return BONGO_CAT_ERROR_MEMORY;
    }
    unsigned char buffer[8192]; size_t count;
    for (;;) {
        if (cancelled && cancelled(userdata)) {
            char scratch[65];
            bongo_safe_sha256_finish(state, scratch);
            fclose(file);
            bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM, "File hash cancelled");
            return BONGO_CAT_ERROR_PLATFORM;
        }
        count = fread(buffer, 1, sizeof(buffer), file);
        if (!count) break;
        bongo_safe_sha256_update(state, buffer, count);
    }
    bool ok = !ferror(file);
    if (fclose(file) != 0) ok = false;
    if (!ok) {
        char scratch[65];
        bongo_safe_sha256_finish(state, scratch);
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Cannot read file");
        return BONGO_CAT_ERROR_IO;
    }
    if (cancelled && cancelled(userdata)) {
        char scratch[65];
        bongo_safe_sha256_finish(state, scratch);
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM, "File hash cancelled");
        return BONGO_CAT_ERROR_PLATFORM;
    }
    bongo_safe_sha256_finish(state, output); return BONGO_CAT_OK;
}
