#include "model_import.h"
#include "model_import_inochi.h"
#include "mver/model_import_mver.h"
#include "tauri/model_import_tauri.h"
#include "bongo_cat/path.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void seed_source_name(const char *source,
    BongoCatImportDiscovery *discovery) {
    char path[BONGO_CAT_PATH_CAP];
    int written = snprintf(path, sizeof(path), "%s", source ? source : "");
    if (written < 0 || (size_t)written >= sizeof(path)) return;
    size_t length = strlen(path);
    while (length && (path[length - 1] == '/' || path[length - 1] == '\\'))
        path[--length] = '\0';
    const char *name = bongo_cat_path_name(path);
    if (name && name[0]) snprintf(discovery->source_name,
        sizeof(discovery->source_name), "%s", name);
}

static int rank(const BongoCatImportCandidate *candidate) {
    return candidate->mode == BONGO_CAT_MODE_STANDARD ? 0 :
        candidate->mode == BONGO_CAT_MODE_KEYBOARD ? 1 : 2;
}

static int format_rank(const BongoCatImportCandidate *candidate) {
    return candidate->format == BONGO_CAT_IMPORT_MVER ? 0 :
        candidate->format == BONGO_CAT_IMPORT_TAURI ? 1 : 2;
}

static int compare_candidates(const void *left, const void *right) {
    const BongoCatImportCandidate *a = left, *b = right;
    int difference = rank(a) - rank(b);
    if (difference) return difference;
    difference = format_rank(a) - format_rank(b);
    if (difference) return difference;
    difference = strcmp(a->directory, b->directory);
    if (difference) return difference;
    difference = strcmp(a->setting, b->setting);
    return difference ? difference : strcmp(a->patch_root, b->patch_root);
}

typedef struct ContainerDiscovery {
    BongoCatImportDiscovery *output;
} ContainerDiscovery;

static BongoCatResult collect_container(void *userdata, const char *source,
    BongoCatImportDiscovery *found, BongoCatError *error) {
    (void)source;
    ContainerDiscovery *container = userdata;
    for (size_t i = 0; i < found->count; ++i) {
        if (container->output->count >= BONGO_CAT_IMPORT_CANDIDATE_CAP) {
            bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
                "Selected folder contains too many model variants");
            return BONGO_CAT_ERROR_FORMAT;
        }
        container->output->candidates[container->output->count++] =
            found->candidates[i];
    }
    return BONGO_CAT_OK;
}

bool bongo_cat_import_discover(const char *source,
    BongoCatImportDiscovery *discovery, BongoCatError *error) {
    memset(discovery, 0, sizeof(*discovery));
    seed_source_name(source, discovery);
    int inochi = bongo_cat_import_inochi_discover(source, discovery, error);
    if (inochi) return inochi > 0;
    /* Broad package discovery intentionally stays separate from exact probe
       dispatch: Mver may be selected from one of its authored subfolders,
       while Tauri conversion starts only from an owning package root. */
    int mver = bongo_cat_import_mver_discover(source, discovery, error);
    if (mver > 0) return true;
    if (mver < 0) return false;
    BongoCatError patch_error = {0};
    int patch = bongo_cat_import_mver_patch_discover(source, discovery,
        &patch_error);
    if (patch > 0) return true;
    memset(discovery, 0, sizeof(*discovery));
    seed_source_name(source, discovery);
    int tauri = bongo_cat_import_tauri_discover_exact(source, discovery, error);
    if (tauri > 0) return true;
    if (tauri < 0) return false;
    memset(discovery, 0, sizeof(*discovery));
    seed_source_name(source, discovery);
    ContainerDiscovery container = {discovery};
    BongoCatResult result = bongo_cat_import_scan(source, collect_container,
        &container, error);
    if (result != BONGO_CAT_OK) return false;
    if (discovery->count) {
        /* Recursive discovery returns the owning package root on each
           candidate. Re-rooting Mver candidates at the selected container
           would copy unrelated sibling folders and break normalization. */
        qsort(discovery->candidates, discovery->count,
            sizeof(discovery->candidates[0]), compare_candidates);
        return true;
    }
    if (patch < 0) {
        if (error) *error = patch_error;
        return false;
    }
    if (error) *error = (BongoCatError){0};
    int recursive = bongo_cat_import_tauri_discover_recursive(source,
        discovery, error);
    if (recursive < 0) return false;
    if (!recursive) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
            "Selected directory contains no valid Live2D or Inochi2D model");
        return false;
    }
    qsort(discovery->candidates, discovery->count,
        sizeof(discovery->candidates[0]), compare_candidates);
    return true;
}
