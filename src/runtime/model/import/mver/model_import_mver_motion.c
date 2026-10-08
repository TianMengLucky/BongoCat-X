#include "model_import_mver_internal.h"
#include "model_import_mver_manifest.h"
#include "bongo_cat/file.h"
#include "bongo_cat/path.h"

#include <stdio.h>
#include "bongo_cat/json_dom.h"

static bool add_rows(BongoJsonMutDoc *output, BongoJsonMutValue *items,
    BongoJsonValue *rows, const BongoCatImportCandidate *candidate,
    const char *kind, const char *field, const char *group, BongoJsonValue *available,
    const BongoCatMverLabels *labels,
    BongoCatError *error) {
    if (!rows || bongo_json_is_null(rows)) return true;
    if (!bongo_json_is_arr(rows)) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
            "Mver %s bindings exceed the matching Live2D manifest entries", kind);
        return false;
    }
    size_t limit = bongo_json_arr_size(rows);
    if (limit > bongo_json_arr_size(available)) limit = bongo_json_arr_size(available);
    size_t index, count; BongoJsonValue *row;
    bongo_json_arr_foreach(rows, index, count, row) {
        if (index >= limit) break;
        if (bongo_json_is_null(row) || (bongo_json_is_arr(row) && !bongo_json_arr_size(row)))
            continue;
        bool disabled = bongo_json_arr_size(row) == 1 &&
            bongo_json_is_int(bongo_json_arr_get_first(row)) &&
            (bongo_json_get_int(bongo_json_arr_get_first(row)) == 0 ||
             bongo_json_get_int(bongo_json_arr_get_first(row)) == 255);
        const char *file = bongo_json_get_str(bongo_json_obj_get(
            bongo_json_arr_get(available, index), "File"));
        char path[BONGO_CAT_PATH_CAP];
        if (!file || !bongo_cat_path_join(path, sizeof(path), candidate->directory,
                file) || !bongo_cat_path_is_file(path)) continue;
        char shortcut[BONGO_CAT_SHORTCUT_CAP] = {0};
        /* Live2D bindings use Windows keys, including in gamepad mode. */
        if (!disabled && !bongo_cat_mver_keyboard_chord(row, shortcut, sizeof(shortcut))) {
            bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
                "Mver %s binding %zu is not a supported input chord", kind, index);
            return false;
        }
        BongoJsonMutValue *item = bongo_json_mut_arr_add_obj(output, items);
        const char *label = bongo_cat_mver_label(labels, field, index);
        if (!item || !bongo_json_mut_obj_add_strcpy(output, item, "kind", kind) ||
            !bongo_json_mut_obj_add_int(output, item, "index", (int)index) ||
            !bongo_json_mut_obj_add_strcpy(output, item, "shortcut", shortcut) ||
            (label && !bongo_json_mut_obj_add_strcpy(output, item, "label", label)) ||
            (group && !bongo_json_mut_obj_add_strcpy(output, item, "group", group))) return false;
    }
    return true;
}

static BongoJsonDoc *manifest(const BongoCatImportCandidate *candidate,
    BongoJsonValue **references) {
    char path[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_path_join(path, sizeof(path), candidate->directory,
        candidate->setting)) return NULL;
    BongoJsonDoc *document = bongo_cat_import_mver_manifest_read(path, NULL);
    *references = document ? bongo_json_obj_get(bongo_json_doc_get_root(document),
        "FileReferences") : NULL;
    return document;
}

static bool add_motion_group(BongoJsonMutDoc *output, BongoJsonMutValue *items,
    BongoJsonValue *configured, BongoJsonValue *motions, const char *field,
    const char *group, const BongoCatImportCandidate *candidate,
    const BongoCatMverLabels *labels, BongoCatError *error) {
    BongoJsonValue *available = bongo_json_obj_get(motions, group);
    if (bongo_json_is_arr(configured) && !bongo_json_is_arr(available)) return true;
    return add_rows(output, items, configured, candidate, "motion", field,
        group, available, labels, error);
}

static BongoJsonValue *binding_rows(BongoJsonValue *config,
    const BongoCatImportCandidate *candidate, const char *field) {
    const char *mode = bongo_cat_mver_binding_mode(
        bongo_cat_mode_name(candidate->mode), field);
    return bongo_json_obj_get(bongo_json_obj_get(config, mode), field);
}

bool bongo_cat_mver_add_behaviors(void *raw_output, void *raw_items,
    void *raw_config, const BongoCatImportCandidate *candidate,
    const BongoCatMverLabels *labels, BongoCatError *error) {
    BongoJsonMutDoc *output = raw_output;
    BongoJsonMutValue *items = raw_items;
    BongoJsonValue *config = raw_config, *references = NULL;
    BongoJsonDoc *document = manifest(candidate, &references);
    BongoJsonValue *expressions = bongo_json_obj_get(references, "Expressions");
    BongoJsonValue *motions = bongo_json_obj_get(references, "Motions");
    bool ok = document && add_rows(output, items,
        binding_rows(config, candidate, "l2d_expression"), candidate, "expression",
        "l2d_expression", NULL,
        expressions, labels, error) &&
        add_motion_group(output, items, binding_rows(config, candidate, "l2d_motion"),
            motions, "l2d_motion", "CAT_motion", candidate, labels, error) &&
        add_motion_group(output, items,
            binding_rows(config, candidate, "l2d_motion_lockhand"), motions,
            "l2d_motion_lockhand", "CAT_motion_lock", candidate, labels, error);
    bongo_json_doc_free(document);
    if (!ok && error && !error->message[0])
        bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
            "Cannot map Mver behaviors to the Live2D manifest");
    return ok;
}
