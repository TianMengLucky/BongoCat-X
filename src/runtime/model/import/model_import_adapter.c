#include "model_import.h"
#include "mver/model_import_mver.h"
#include "model_storage.h"
#include "runtime.h"
#include "bongo_cat/file.h"
#include "bongo_cat/path.h"

#include <SDL3/SDL.h>
#include <stdio.h>

static bool copy_optional(const char *source_dir, const char *source_name,
    const char *target_dir, const char *target_name) {
    char source[BONGO_CAT_PATH_CAP], target[BONGO_CAT_PATH_CAP];
    return bongo_cat_path_join(source, sizeof(source), source_dir, source_name) &&
        bongo_cat_path_join(target, sizeof(target), target_dir, target_name) &&
        bongo_cat_path_copy_file(source, target);
}

static bool copy_first(const char *source_dir, const char *const *names,
    size_t count, const char *target_dir, const char *target_name) {
    for (size_t i = 0; i < count; ++i) {
        char source[BONGO_CAT_PATH_CAP];
        if (!bongo_cat_path_join(source, sizeof(source), source_dir, names[i]) ||
            !bongo_cat_path_is_file(source)) continue;
        return copy_optional(source_dir, names[i], target_dir, target_name);
    }
    return true;
}

static bool copy_preview_file(const char *source_resources,
    const char *source_root, const char *const *names, size_t count,
    const char *target_resources, const char *target_name) {
    for (size_t i = 0; i < count; ++i) {
        char source[BONGO_CAT_PATH_CAP];
        if (bongo_cat_path_join(source, sizeof(source), source_resources,
                names[i]) && bongo_cat_path_is_file(source))
            return copy_optional(source_resources, names[i], target_resources,
                target_name);
    }
    return copy_first(source_root, names, count, target_resources, target_name);
}

static bool preview_file_exists(const char *directory, const char *name) {
    char path[BONGO_CAT_PATH_CAP];
    return bongo_cat_path_join(path, sizeof(path), directory, name) &&
        bongo_cat_path_is_file(path);
}

static bool copy_preview(const BongoCatImportCandidate *candidate,
    const char *target, BongoCatError *error) {
    char source_resources[BONGO_CAT_PATH_CAP], target_resources[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_path_join(source_resources, sizeof(source_resources),
        candidate->assets, "resources") ||
        !bongo_cat_path_join(target_resources, sizeof(target_resources), target,
            "resources")) return false;
    bool target_exists = bongo_cat_path_is_dir(target_resources);
    if (bongo_cat_path_is_dir(source_resources) && !target_exists) {
        if (bongo_cat_model_copy_directory(source_resources, target_resources,
                error) != BONGO_CAT_OK) return false;
        target_exists = true;
    }
    if (!target_exists && !bongo_cat_path_create_directory(target_resources))
        return false;
    const char *covers[] = {
        "cover.png", "cat.png", "bg.png", "mousebg.png", "tabletbg.png"};
    const char *backgrounds[] = {
        "background.png", "bg.png", "mousebg.png", "tabletbg.png"};
    bool ok = (preview_file_exists(target_resources, "cover.png") ||
        copy_preview_file(source_resources, candidate->assets, covers, 5,
            target_resources, "cover.png")) &&
        (preview_file_exists(target_resources, "background.png") ||
        copy_preview_file(source_resources, candidate->assets, backgrounds, 4,
            target_resources, "background.png"));
    if (!ok) bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
        "Cannot copy model preview assets: %s", SDL_GetError());
    return ok;
}

static bool write_mode(const char *target, BongoCatModelMode mode,
    BongoCatError *error) {
    char path[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_path_join(path, sizeof(path), target, ".bongo-cat-mode"))
        return false;
    FILE *file = bongo_cat_file_open(path, "wb");
    if (!file) return false;
    const char *name = mode == BONGO_CAT_MODE_KEYBOARD ? "keyboard" :
        mode == BONGO_CAT_MODE_GAMEPAD ? "gamepad" : "standard";
    bool ok = fputs(name, file) >= 0;
    if (fclose(file) != 0) ok = false;
    if (!ok) bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
        "Cannot write imported model metadata");
    return ok;
}

bool bongo_cat_import_prepare_adapter(const BongoCatImportCandidate *candidate,
    const char *target, BongoCatError *error) {
    if (!candidate || !target || (!bongo_cat_path_is_dir(target) &&
        !bongo_cat_path_create_directory(target))) return false;
    return copy_preview(candidate, target, error) &&
        (candidate->format == BONGO_CAT_IMPORT_INOCHI2D ||
            bongo_cat_import_mver_assets(candidate, target, error)) &&
        bongo_cat_import_adapter_metadata(candidate, target, error) &&
        bongo_cat_import_write_report(candidate, target, error) &&
        write_mode(target, candidate->mode, error);
}
