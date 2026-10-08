#include "model_import_inochi.h"
#include "model_import_path.h"
#include "model_storage.h"
#include "bongo_cat/path.h"
#include "bongo_cat/safe_ffi.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

typedef struct InochiScan {
    BongoCatImportDiscovery *discovery;
    BongoCatError *error;
    bool live2d, selected_file;
} InochiScan;

static bool inochi_name(const char *name) {
    return bongo_cat_import_has_suffix_ci(name, ".inp") ||
        bongo_cat_import_has_suffix_ci(name, ".inx");
}
static BongoCatPathVisit discover_file(void *userdata,
    const char *directory, const char *name) {
    InochiScan *scan = userdata;
    char path[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_path_join(path, sizeof(path), directory, name))
        return BONGO_CAT_PATH_FAILURE;
    if (!bongo_cat_path_is_file(path)) return BONGO_CAT_PATH_CONTINUE;
    bool live_name = bongo_cat_import_has_suffix_ci(name, ".model3.json");
    if (!scan->selected_file && !live_name && !inochi_name(name))
        return BONGO_CAT_PATH_CONTINUE;
    int kind = bongo_safe_model_kind_file(path);
    if (kind == BONGO_CAT_MODEL_ENGINE_LIVE2D) {
        scan->live2d = true;
        return BONGO_CAT_PATH_CONTINUE;
    }
    if (kind != BONGO_CAT_MODEL_ENGINE_INOX2D && !inochi_name(name))
        return BONGO_CAT_PATH_CONTINUE;
    char message[256] = {0};
    if (kind != BONGO_CAT_MODEL_ENGINE_INOX2D ||
        !bongo_safe_inochi_validate_file(path, message, sizeof(message))) {
        bongo_cat_error_set(scan->error, BONGO_CAT_ERROR_FORMAT,
            "Invalid Inochi2D model %s: %s", name,
            message[0] ? message : "file contents do not match INP/INX");
        return BONGO_CAT_PATH_FAILURE;
    }
    if (scan->discovery->count >= BONGO_CAT_IMPORT_CANDIDATE_CAP) {
        bongo_cat_error_set(scan->error, BONGO_CAT_ERROR_FORMAT,
            "Too many Inochi2D models in the selected directory");
        return BONGO_CAT_PATH_FAILURE;
    }
    BongoCatImportCandidate *candidate =
        &scan->discovery->candidates[scan->discovery->count++];
    memset(candidate, 0, sizeof(*candidate));
    candidate->format = BONGO_CAT_IMPORT_INOCHI2D;
    candidate->mode = BONGO_CAT_MODE_STANDARD;
    SDL_strlcpy(candidate->directory, directory, sizeof(candidate->directory));
    SDL_strlcpy(candidate->package_root, directory, sizeof(candidate->package_root));
    SDL_strlcpy(candidate->assets, directory, sizeof(candidate->assets));
    SDL_strlcpy(candidate->setting, name, sizeof(candidate->setting));
    return BONGO_CAT_PATH_CONTINUE;
}

int bongo_cat_import_inochi_discover(const char *source,
    BongoCatImportDiscovery *discovery, BongoCatError *error) {
    InochiScan scan = {discovery, error, false, false};
    if (bongo_cat_path_is_file(source)) {
        scan.selected_file = true;
        char parent[BONGO_CAT_PATH_CAP];
        if (!bongo_cat_import_parent_path(source, parent, sizeof(parent)) ||
            discover_file(&scan, parent, bongo_cat_path_name(source)) == BONGO_CAT_PATH_FAILURE)
            return -1;
    } else if (bongo_cat_path_is_dir(source)) {
        if (!bongo_cat_path_enumerate(source, discover_file, &scan)) return -1;
        if (discovery->count && scan.live2d) {
            bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
                "Mixed Live2D and Inochi2D files: select a model file or separate model folders");
            return -1;
        }
    }
    return discovery->count ? 1 : 0;
}

typedef struct InochiCopy { const char *target; const char *setting; } InochiCopy;
static BongoCatPathVisit copy_file(void *userdata, const char *directory, const char *name) {
    InochiCopy *copy = userdata;
    if (strcmp(name, copy->setting) && SDL_strcasecmp(name, "bongocat.bindings.json") &&
        SDL_strcasecmp(name, "cover.png") && SDL_strcasecmp(name, "background.png"))
        return BONGO_CAT_PATH_CONTINUE;
    char source[BONGO_CAT_PATH_CAP], target[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_path_join(source, sizeof(source), directory, name) ||
        !bongo_cat_path_join(target, sizeof(target), copy->target, name) ||
        !bongo_cat_path_copy_file(source, target)) return BONGO_CAT_PATH_FAILURE;
    return BONGO_CAT_PATH_CONTINUE;
}
bool bongo_cat_import_inochi_copy(const BongoCatImportCandidate *candidate,
    const char *target, BongoCatImportCandidate *installed, BongoCatError *error) {
    InochiCopy copy = {target, candidate->setting};
    if (!bongo_cat_path_enumerate(candidate->directory, copy_file, &copy)) return false;
    char source[BONGO_CAT_PATH_CAP], destination[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_path_join(source, sizeof(source), candidate->directory, "resources") ||
        !bongo_cat_path_join(destination, sizeof(destination), target, "resources")) return false;
    if (bongo_cat_path_is_dir(source) && !bongo_cat_path_is_dir(destination) &&
        bongo_cat_model_copy_directory(source, destination, error) != BONGO_CAT_OK) return false;
    SDL_strlcpy(installed->directory, target, sizeof(installed->directory));
    SDL_strlcpy(installed->assets, target, sizeof(installed->assets));
    SDL_strlcpy(installed->package_root, target, sizeof(installed->package_root));
    return true;
}
