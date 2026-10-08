#include "model_import_mver_internal.h"
#include "bongo_cat/image.h"
#include "bongo_cat/path.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include "bongo_cat/json_dom.h"

static bool effect_source(const BongoCatImportCandidate *candidate, const char *name,
    char *output, size_t capacity) {
    char directory[BONGO_CAT_PATH_CAP];
    if (candidate->overrides[0] &&
        bongo_cat_path_join(directory, sizeof(directory), candidate->overrides, "face") &&
        bongo_cat_path_join(output, capacity, directory, name) &&
        bongo_cat_path_is_file(output)) return true;
    return bongo_cat_path_join(directory, sizeof(directory), candidate->assets, "face") &&
        bongo_cat_path_join(output, capacity, directory, name) &&
        bongo_cat_path_is_file(output);
}

static bool clear_binding(BongoJsonMutDoc *output, BongoJsonMutValue *items,
    BongoJsonValue *root, const BongoCatImportCandidate *candidate) {
    BongoJsonValue *decoration = bongo_json_obj_get(root, "decoration");
    BongoJsonValue *row = bongo_json_obj_get(decoration, "emoticonClear");
    if (!row || bongo_json_is_null(row) ||
        (bongo_json_is_arr(row) && !bongo_json_arr_size(row))) return true;
    char shortcut[BONGO_CAT_SHORTCUT_CAP];
    if (!bongo_cat_mver_chord(candidate, row, shortcut, sizeof(shortcut))) return false;
    BongoJsonMutValue *item = bongo_json_mut_arr_add_obj(output, items);
    return item && bongo_json_mut_obj_add_str(output, item, "kind", "effect-clear") &&
        bongo_json_mut_obj_add_strcpy(output, item, "shortcut", shortcut);
}

bool bongo_cat_mver_effects(void *raw_output, void *raw_items, void *raw_root,
    void *raw_mode,
    const BongoCatImportCandidate *candidate, const char *target) {
    BongoJsonMutDoc *output = raw_output;
    BongoJsonMutValue *items = raw_items;
    BongoJsonValue *root = raw_root;
    BongoJsonValue *mode = raw_mode;
    BongoJsonValue *rows = bongo_json_obj_get(mode, "face");
    if (!bongo_json_is_arr(rows) || !bongo_json_arr_size(rows)) return true;
    BongoJsonValue *decoration = bongo_json_obj_get(root, "decoration");
    bool momentary = !bongo_json_get_bool(bongo_json_obj_get(decoration, "emoticonKeep"));
    size_t emitted = 0;
    char resources[BONGO_CAT_PATH_CAP], target_effects[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_path_join(resources, sizeof(resources), target, "resources") ||
        !bongo_cat_path_join(target_effects, sizeof(target_effects), resources, "effects") ||
        !bongo_cat_path_create_directory(target_effects)) return false;
    size_t index, count; BongoJsonValue *row;
    bongo_json_arr_foreach(rows, index, count, row) {
        char shortcut[BONGO_CAT_SHORTCUT_CAP], name[32];
        char source[BONGO_CAT_PATH_CAP], destination[BONGO_CAT_PATH_CAP];
        if (bongo_json_is_null(row) || (bongo_json_is_arr(row) && !bongo_json_arr_size(row)))
            continue;
        snprintf(name, sizeof(name), "%zu.png", index);
        if (!effect_source(candidate, name, source, sizeof(source)) ||
            !bongo_cat_image_info(source, NULL, NULL)) continue;
        if (!bongo_cat_mver_chord(candidate, row, shortcut, sizeof(shortcut))) return false;
        if (!bongo_cat_path_join(destination, sizeof(destination), target_effects, name) ||
            !bongo_cat_path_copy_file(source, destination)) return false;
        BongoJsonMutValue *item = bongo_json_mut_arr_add_obj(output, items);
        char relative[BONGO_CAT_PATH_CAP];
        snprintf(relative, sizeof(relative), "resources/effects/%s", name);
        if (!item || !bongo_json_mut_obj_add_str(output, item, "kind", "effect") ||
            !bongo_json_mut_obj_add_int(output, item, "index", (int)index) ||
            !bongo_json_mut_obj_add_strcpy(output, item, "shortcut", shortcut) ||
            !bongo_json_mut_obj_add_strcpy(output, item, "effect", relative) ||
            (momentary && !bongo_json_mut_obj_add_bool(output, item, "momentary", true))) return false;
        emitted++;
    }
    return !emitted || momentary || clear_binding(output, items, root, candidate);
}
