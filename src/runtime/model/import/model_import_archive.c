#include "model_import_archive.h"
#include "model_import_path.h"
#include "model_storage.h"
#include "bongo_cat/path.h"
#include "bongo_cat/safe_archive.h"
#include "bongo_cat/log.h"
#include <SDL3/SDL.h>

bool bongo_cat_import_is_archive(const char *source) {
    return source && bongo_cat_import_has_suffix_ci(source, ".zip") &&
        !bongo_cat_path_is_dir(source);
}

void bongo_cat_import_archive_cleanup(const char *temporary) {
    if (temporary && temporary[0] && !bongo_cat_model_remove_tree(temporary, NULL))
        SDL_LogWarn(BONGO_CAT_LOG_LIFECYCLE,
            "Cannot remove model archive temporary directory: %s", temporary);
}

BongoCatResult bongo_cat_import_archive_extract(const char *source,
    char directory[BONGO_CAT_PATH_CAP], char temporary[BONGO_CAT_PATH_CAP],
    BongoCatError *error) {
    if (!source || !directory || !temporary) return BONGO_CAT_ERROR_ARGUMENT;
    directory[0] = temporary[0] = '\0';
    char *base = SDL_GetPrefPath("BongoCat", "archive-import");
    char message[256] = {0};
    BongoCatResult result = base ? (BongoCatResult)bongo_safe_archive_extract(source,
        base, directory, temporary, BONGO_CAT_PATH_CAP, message, sizeof(message)) :
        BONGO_CAT_ERROR_IO;
    SDL_free(base);
    if (result != BONGO_CAT_OK)
        bongo_cat_error_set(error, result, "Cannot extract ZIP: %s (%s)", source, message);
    return result;
}
