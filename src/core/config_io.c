#include "config_internal.h"
#include "bongo_cat/path.h"
#include "bongo_cat/safe_ffi.h"

#include <stdlib.h>
#include <string.h>

/* Maps the bongo-safe parse result onto the public result codes. The -1
   layout sentinel means the mirrored struct definitions drifted from
   bongo_cat/config.h — a build-time bug that must fail loudly. */
static BongoCatResult map_parse_result(int result, const char *message,
    BongoCatError *error) {
    BongoCatResult mapped = result == -1 ? BONGO_CAT_ERROR_PLATFORM :
        result == 2 ? BONGO_CAT_ERROR_IO :
        result == 3 ? BONGO_CAT_ERROR_FORMAT : BONGO_CAT_ERROR_MEMORY;
    bongo_cat_error_set(error, mapped, "%s",
        result == -1 ? "Configuration struct layout mismatch" :
        message && message[0] ? message : "Invalid configuration");
    return mapped;
}

static BongoCatResult load_document(const char *path, bool settings_format,
    void *loaded, size_t loaded_size, bool *present, BongoCatError *error) {
    *present = false;
    if (!bongo_cat_path_is_file(path)) return BONGO_CAT_OK;
    unsigned char *bytes = NULL;
    size_t length = 0;
    BongoCatResult result = bongo_cat_config_read_bytes(path, &bytes,
        &length, error);
    if (result != BONGO_CAT_OK) return result;
    char message[256] = {0};
    int parsed = settings_format ?
        bongo_safe_settings_parse(bytes, length, loaded, loaded_size,
            message, sizeof(message)) :
        bongo_safe_session_parse(bytes, length, loaded, loaded_size,
            message, sizeof(message));
    free(bytes);
    if (parsed != 0) return map_parse_result(parsed, message, error);
    *present = true;
    return BONGO_CAT_OK;
}

BongoCatResult bongo_cat_settings_load(const char *path,
    BongoCatSettings *settings, BongoCatError *error) {
    if (!path || !settings) return BONGO_CAT_ERROR_ARGUMENT;
    BongoCatSettings loaded = *settings;
    bool present = false;
    BongoCatResult result = load_document(path, true, &loaded,
        sizeof(loaded), &present, error);
    if (result != BONGO_CAT_OK) return result;
    if (present) {
        bongo_cat_settings_validate(&loaded);
        *settings = loaded;
    }
    return BONGO_CAT_OK;
}

BongoCatResult bongo_cat_session_load(const char *path,
    BongoCatSessionState *session, BongoCatError *error) {
    if (!path || !session) return BONGO_CAT_ERROR_ARGUMENT;
    BongoCatSessionState loaded = *session;
    bool present = false;
    BongoCatResult result = load_document(path, false, &loaded,
        sizeof(loaded), &present, error);
    if (result != BONGO_CAT_OK) return result;
    if (present) {
        bongo_cat_session_validate(&loaded);
        *session = loaded;
    }
    return BONGO_CAT_OK;
}

static BongoCatResult save_document(const char *path, const char *description,
    const void *value, size_t value_size, bool settings, BongoCatError *error) {
    unsigned char *json = NULL;
    size_t length = 0;
    int result = settings ?
        bongo_safe_settings_write(value, value_size, &json, &length) :
        bongo_safe_session_write(value, value_size, &json, &length);
    if (result != 0) {
        bongo_cat_error_set(error,
            result == -1 ? BONGO_CAT_ERROR_PLATFORM :
            result == 4 ? BONGO_CAT_ERROR_MEMORY : BONGO_CAT_ERROR_FORMAT,
            "Cannot serialize %s%s", description,
            result == -1 ? " (configuration struct layout mismatch)" : "");
        return result == -1 ? BONGO_CAT_ERROR_PLATFORM :
            result == 4 ? BONGO_CAT_ERROR_MEMORY : BONGO_CAT_ERROR_FORMAT;
    }
    BongoCatResult written = bongo_cat_config_write_bytes(path, json, length,
        description, error);
    bongo_safe_free_json(json, length);
    return written;
}

BongoCatResult bongo_cat_settings_save(const char *path,
    const BongoCatSettings *settings, BongoCatError *error) {
    if (!path || !settings) return BONGO_CAT_ERROR_ARGUMENT;
    BongoCatSettings canonical = *settings;
    bongo_cat_settings_validate(&canonical);
    return save_document(path, "settings file", &canonical,
        sizeof(canonical), true, error);
}

BongoCatResult bongo_cat_session_save(const char *path,
    const BongoCatSessionState *session, BongoCatError *error) {
    if (!path || !session) return BONGO_CAT_ERROR_ARGUMENT;
    BongoCatSessionState canonical = *session;
    bongo_cat_session_validate(&canonical);
    return save_document(path, "session file", &canonical,
        sizeof(canonical), false, error);
}
