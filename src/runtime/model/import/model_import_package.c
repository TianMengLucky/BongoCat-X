#include "model_import.h"
#include "model_import_inochi.h"
#include "model_import_path.h"
#include "model_import_mver_copy.h"
#include "tauri/model_import_tauri.h"
#include "bongo_cat/path.h"

#include <stdio.h>
#include <string.h>

static bool copy_package_files(const BongoCatImportCandidate *candidate,
    const char *target, BongoCatImportCandidate *installed,
    BongoCatError *error) {
    if (candidate->format == BONGO_CAT_IMPORT_INOCHI2D)
        return bongo_cat_import_inochi_copy(candidate, target, installed, error);
    if (candidate->format == BONGO_CAT_IMPORT_TAURI)
        return bongo_cat_import_tauri_convert_to_mver(candidate, target,
            installed, error);
    return bongo_cat_import_mver_copy_package(candidate, target, installed,
        error);
}

bool bongo_cat_import_prepare_package(const BongoCatImportCandidate *candidate,
    const char *target, BongoCatImportCandidate *installed,
    BongoCatError *error) {
    if (!candidate || !target || !installed ||
        !bongo_cat_path_create_directory(target)) return false;
    *installed = *candidate;
    if (copy_package_files(candidate, target, installed, error)) return true;
    if (error && !error->message[0]) bongo_cat_error_set(error,
        BONGO_CAT_ERROR_IO, "Cannot preserve imported model files");
    return false;
}

static bool tauri_variant_directory(char *output, size_t capacity,
    const char *target, const BongoCatImportDiscovery *discovery,
    size_t index) {
    const char *mode = bongo_cat_mode_name(discovery->candidates[index].mode);
    size_t occurrence = 1;
    for (size_t i = 0; i < index; ++i)
        if (discovery->candidates[i].mode == discovery->candidates[index].mode)
            occurrence++;
    char name[48];
    int written = occurrence == 1
        ? snprintf(name, sizeof(name), "%s", mode)
        : snprintf(name, sizeof(name), "%s-%zu", mode, occurrence);
    return written >= 0 && (size_t)written < sizeof(name) &&
        bongo_cat_path_join(output, capacity, target, name);
}

static bool common_package_root(const BongoCatImportDiscovery *discovery,
    char root[BONGO_CAT_PATH_CAP]) {
    if (!bongo_cat_import_parent_path(discovery->candidates[0].package_root,
            root, BONGO_CAT_PATH_CAP)) return false;
    for (size_t i = 1; i < discovery->count; ++i) {
        char relative[BONGO_CAT_PATH_CAP];
        while (!bongo_cat_import_relative_path(root,
                discovery->candidates[i].package_root, relative,
                sizeof(relative))) {
            char parent[BONGO_CAT_PATH_CAP];
            if (!bongo_cat_import_parent_path(root, parent, sizeof(parent)))
                return false;
            snprintf(root, BONGO_CAT_PATH_CAP, "%s", parent);
        }
    }
    return true;
}

static bool prepare_container_storage(
    const BongoCatImportDiscovery *discovery, const char *target,
    BongoCatError *error) {
    char root[BONGO_CAT_PATH_CAP];
    if (!common_package_root(discovery, root)) return false;
    for (size_t i = 0; i < discovery->count; ++i) {
        bool already_copied = false;
        for (size_t j = 0; j < i; ++j)
            if (strcmp(discovery->candidates[j].package_root,
                    discovery->candidates[i].package_root) == 0) {
                already_copied = true;
                break;
            }
        if (already_copied) continue;
        char relative[BONGO_CAT_PATH_CAP], package[BONGO_CAT_PATH_CAP];
        if (!bongo_cat_import_relative_path(root,
                discovery->candidates[i].package_root, relative,
                sizeof(relative)) || !relative[0] ||
            !bongo_cat_path_join(package, sizeof(package), target, relative))
            return false;
        BongoCatImportCandidate installed;
        if (!bongo_cat_import_prepare_package(&discovery->candidates[i],
                package, &installed, error)) return false;
    }
    return true;
}

static bool single_package_root(const BongoCatImportDiscovery *discovery) {
    for (size_t i = 1; i < discovery->count; ++i)
        if (strcmp(discovery->candidates[0].package_root,
                discovery->candidates[i].package_root) != 0) return false;
    return true;
}

bool bongo_cat_import_prepare_storage(
    const BongoCatImportDiscovery *discovery, const char *target,
    BongoCatError *error) {
    if (!discovery || !discovery->count || !target) return false;
    bool tauri = true;
    for (size_t i = 0; i < discovery->count; ++i)
        tauri = tauri && discovery->candidates[i].format ==
            BONGO_CAT_IMPORT_TAURI;
    if (discovery->candidates[0].format == BONGO_CAT_IMPORT_INOCHI2D &&
        single_package_root(discovery)) {
        for (size_t i = 0; i < discovery->count; ++i) {
            BongoCatImportCandidate installed;
            if (!bongo_cat_import_prepare_package(&discovery->candidates[i],
                target, &installed, error)) return false;
        }
        return true;
    }
    if (!tauri && single_package_root(discovery)) {
        BongoCatImportCandidate installed;
        return bongo_cat_import_prepare_package(&discovery->candidates[0],
            target, &installed, error);
    }
    if (!tauri) return prepare_container_storage(discovery, target, error);
    for (size_t i = 0; i < discovery->count; ++i) {
        char package[BONGO_CAT_PATH_CAP];
        if (discovery->count == 1)
            snprintf(package, sizeof(package), "%s", target);
        else if (!tauri_variant_directory(package, sizeof(package), target,
                discovery, i)) return false;
        BongoCatImportCandidate installed;
        if (!bongo_cat_import_prepare_package(&discovery->candidates[i],
                package, &installed, error)) return false;
    }
    return true;
}

bool bongo_cat_import_authored_package(const char *directory) {
    char path[BONGO_CAT_PATH_CAP];
    if (!directory || !bongo_cat_path_is_dir(directory)) return false;
    static const char *const internal[] = {
        ".bongo-cat-package.json", ".bongo-cat-adapter",
        BONGO_CAT_MODEL_BUILTIN_MARKER
    };
    for (size_t i = 0; i < sizeof(internal) / sizeof(internal[0]); ++i)
        if (bongo_cat_path_join(path, sizeof(path), directory, internal[i]) &&
            (bongo_cat_path_is_file(path) || bongo_cat_path_is_dir(path)))
            return false;
    return true;
}
