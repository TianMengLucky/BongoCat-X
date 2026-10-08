#include "model_import_mver_internal.h"
#include "bongo_cat/path.h"

#include <stdio.h>
#include "bongo_cat/json_dom.h"

static bool sound_source(const BongoCatImportCandidate *candidate, size_t index,
    char *source, size_t capacity, char *relative, size_t relative_capacity) {
    static const char *extensions[] = {"wav", "ogg", "flac"};
    for (size_t i = 0; i < sizeof(extensions) / sizeof(extensions[0]); ++i) {
        char name[40], sounds[BONGO_CAT_PATH_CAP];
        snprintf(name, sizeof(name), "%zu.%s", index, extensions[i]);
        if (!bongo_cat_path_join(sounds, sizeof(sounds), candidate->assets, "sounds") ||
            !bongo_cat_path_join(source, capacity, sounds, name) ||
            !bongo_cat_path_is_file(source)) continue;
        snprintf(relative, relative_capacity, "resources/sounds/%s", name);
        return true;
    }
    return false;
}

static bool add_sound_clear(BongoJsonMutDoc *output, BongoJsonMutValue *items,
    BongoJsonValue *config) {
    BongoJsonValue *decoration = bongo_json_obj_get(config, "decoration");
    BongoJsonValue *row = bongo_json_obj_get(decoration, "soundClear");
    if (!row || (bongo_json_is_arr(row) && bongo_json_arr_size(row) == 0)) return true;
    BongoJsonMutValue *item = bongo_json_mut_arr_add_obj(output, items);
    /* Stopping all audio is an explicit command and has no default shortcut. */
    return item && bongo_json_mut_obj_add_str(output, item, "kind", "sound-clear");
}

bool bongo_cat_mver_add_audio(void *raw_output, void *raw_items,
    void *raw_config, void *raw_rows,
    const BongoCatImportCandidate *candidate, const BongoCatMverLabels *labels,
    const char *target) {
    BongoJsonMutDoc *output = raw_output;
    BongoJsonMutValue *items = raw_items;
    BongoJsonValue *config = raw_config, *rows = raw_rows;
    if (!rows || bongo_json_is_null(rows)) return true;
    if (!bongo_json_is_arr(rows)) return false;
    char target_resources[BONGO_CAT_PATH_CAP], target_sounds[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_path_join(target_resources, sizeof(target_resources), target, "resources") ||
        !bongo_cat_path_join(target_sounds, sizeof(target_sounds), target_resources, "sounds") ||
        !bongo_cat_path_create_directory(target_sounds)) return false;
    BongoJsonValue *decoration = bongo_json_obj_get(config, "decoration");
    BongoJsonValue *keep_value = bongo_json_obj_get(decoration, "soundKeep");
    bool keep = !keep_value || bongo_json_get_bool(keep_value);
    size_t emitted = 0, index, count; BongoJsonValue *row;
    bongo_json_arr_foreach(rows, index, count, row) {
        char shortcut[BONGO_CAT_SHORTCUT_CAP], source[BONGO_CAT_PATH_CAP];
        char relative[BONGO_CAT_PATH_CAP], destination[BONGO_CAT_PATH_CAP];
        if (bongo_json_is_null(row) || (bongo_json_is_arr(row) && !bongo_json_arr_size(row)))
            continue;
        if (!sound_source(candidate, index, source, sizeof(source), relative,
            sizeof(relative))) continue;
        if (!bongo_cat_mver_sound_chord(row, shortcut, sizeof(shortcut))) return false;
        const char *name = bongo_cat_path_name(source);
        if (!bongo_cat_path_join(destination, sizeof(destination), target_sounds, name) ||
            !bongo_cat_path_copy_file(source, destination)) return false;
        BongoJsonMutValue *item = bongo_json_mut_arr_add_obj(output, items);
        const char *label = bongo_cat_mver_label(labels, "sounds", index);
        if (!item || !bongo_json_mut_obj_add_str(output, item, "kind", "sound") ||
            !bongo_json_mut_obj_add_strcpy(output, item, "shortcut", shortcut) ||
            !bongo_json_mut_obj_add_strcpy(output, item, "sound", relative) ||
            (label && !bongo_json_mut_obj_add_strcpy(output, item, "label", label)) ||
            !bongo_json_mut_obj_add_bool(output, item, "overlap", keep)) return false;
        emitted++;
    }
    return !emitted || add_sound_clear(output, items, config);
}

