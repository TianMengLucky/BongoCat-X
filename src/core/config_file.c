#include "config_internal.h"
#include "bongo_cat/file.h"
#include "bongo_cat/path.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include "windows_utf8.h"
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

BongoCatResult bongo_cat_config_read_bytes(const char *path,
    unsigned char **bytes, size_t *length, BongoCatError *error) {
    *bytes = NULL;
    *length = 0;
    uint64_t file_size = 0;
    if (!bongo_cat_path_file_size(path, &file_size)) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
            "Cannot inspect configuration file: %s", path);
        return BONGO_CAT_ERROR_IO;
    }
    if (!file_size || file_size > BONGO_CAT_CONFIG_FILE_CAP) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
            "Configuration file size must be between 1 and %u bytes",
            (unsigned)BONGO_CAT_CONFIG_FILE_CAP);
        return BONGO_CAT_ERROR_FORMAT;
    }
    FILE *file = bongo_cat_file_open(path, "rb");
    if (!file) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
            "Cannot open configuration file: %s", path);
        return BONGO_CAT_ERROR_IO;
    }
    unsigned char *data = malloc((size_t)file_size);
    size_t read = data ? fread(data, 1, (size_t)file_size, file) : 0;
    bool failed = !data || read != (size_t)file_size || ferror(file);
    if (fclose(file) != 0) failed = true;
    if (failed) {
        free(data);
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
            "Cannot read configuration file: %s", path);
        return BONGO_CAT_ERROR_IO;
    }
    *bytes = data;
    *length = (size_t)file_size;
    return BONGO_CAT_OK;
}

static bool sync_file(const char *path) {
#ifdef _WIN32
    wchar_t *wide = bongo_cat_windows_wide(path);
    HANDLE file = wide ? CreateFileW(wide, GENERIC_WRITE, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL) : INVALID_HANDLE_VALUE;
    free(wide);
    if (file == INVALID_HANDLE_VALUE) return false;
    bool result = FlushFileBuffers(file) != FALSE;
    CloseHandle(file);
    return result;
#else
    int file = open(path, O_RDONLY);
    if (file < 0) return false;
    bool result = fsync(file) == 0;
    close(file);
    return result;
#endif
}

#ifndef _WIN32
static bool sync_parent(const char *path) {
    char directory[BONGO_CAT_PATH_CAP];
    int length = snprintf(directory, sizeof(directory), "%s", path);
    if (length < 0 || (size_t)length >= sizeof(directory)) return false;
    char *slash = strrchr(directory, '/');
    if (!slash) snprintf(directory, sizeof(directory), ".");
    else if (slash == directory) slash[1] = '\0';
    else *slash = '\0';
    int handle = open(directory, O_RDONLY);
    if (handle < 0) return false;
    bool result = fsync(handle) == 0;
    close(handle);
    return result;
}
#endif

BongoCatResult bongo_cat_config_write_bytes(const char *path,
    const unsigned char *bytes, size_t length, const char *description,
    BongoCatError *error) {
    char temporary[BONGO_CAT_PATH_CAP + 8];
    int written = snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    if (written < 0 || (size_t)written >= sizeof(temporary)) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_ARGUMENT,
            "Configuration path is too long");
        return BONGO_CAT_ERROR_ARGUMENT;
    }
    FILE *file = bongo_cat_file_open(temporary, "wb");
    bool ok = file && fwrite(bytes, 1, length, file) == length;
    if (file && fclose(file) != 0) ok = false;
    if (!ok) {
        bongo_cat_file_remove(temporary);
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
            "Cannot write %s", description);
        return BONGO_CAT_ERROR_IO;
    }
    if (!sync_file(temporary)) {
        bongo_cat_file_remove(temporary);
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
            "Cannot flush %s", description);
        return BONGO_CAT_ERROR_IO;
    }
    if (!bongo_cat_file_replace(temporary, path, true)) {
        bongo_cat_file_remove(temporary);
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
            "Cannot replace %s", description);
        return BONGO_CAT_ERROR_IO;
    }
#ifndef _WIN32
    if (!sync_parent(path)) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
            "Cannot flush configuration directory");
        return BONGO_CAT_ERROR_IO;
    }
#endif
    return BONGO_CAT_OK;
}
