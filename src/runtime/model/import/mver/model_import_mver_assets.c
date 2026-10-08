#include "model_import_mver_internal.h"
#include "runtime.h"
#include "bongo_cat/file.h"
#include "bongo_cat/image.h"
#include "bongo_cat/path.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bongo_cat/json_dom.h"

typedef BongoCatMverKeyNames KeyNames;

static bool asset_file(const BongoCatImportCandidate *candidate, const char *group,
    const char *name, char *output, size_t capacity) {
    char directory[BONGO_CAT_PATH_CAP];
    if (candidate->overrides[0] &&
        bongo_cat_path_join(directory, sizeof(directory), candidate->overrides, group) &&
        bongo_cat_path_join(output, capacity, directory, name) &&
        bongo_cat_path_is_file(output)) return true;
    return bongo_cat_path_join(directory, sizeof(directory), candidate->assets, group) &&
        bongo_cat_path_join(output, capacity, directory, name) &&
        bongo_cat_path_is_file(output);
}

static bool root_asset_file(const BongoCatImportCandidate *candidate,
    const char *name, char *output, size_t capacity) {
    if (candidate->overrides[0] &&
        bongo_cat_path_join(output, capacity, candidate->overrides, name) &&
        bongo_cat_path_is_file(output)) return true;
    return bongo_cat_path_join(output, capacity, candidate->assets, name) &&
        bongo_cat_path_is_file(output);
}

static bool copy_standard_pointer_assets(const BongoCatImportCandidate *candidate,
    const char *target) {
    static const char *const names[] = {
        "arm.png", "mouse.png", "mouse_left.png", "mouse_right.png",
        "mouse_side.png", "tablet.png", "tablet_left.png", "tablet_right.png"
    };
    char resources[BONGO_CAT_PATH_CAP], output[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_path_join(resources, sizeof(resources), target, "resources") ||
        !bongo_cat_path_join(output, sizeof(output), resources, "mver-pointer") ||
        !bongo_cat_path_create_directory(output)) return false;
    for (size_t index = 0; index < sizeof(names) / sizeof(names[0]); ++index) {
        char source[BONGO_CAT_PATH_CAP], destination[BONGO_CAT_PATH_CAP];
        if (!root_asset_file(candidate, names[index], source, sizeof(source))) continue;
        int width = 0, height = 0;
        if (!bongo_cat_image_info(source, &width, &height) ||
            (width <= 1 && height <= 1)) continue;
        if (!bongo_cat_path_join(destination, sizeof(destination), output, names[index]) ||
            !bongo_cat_path_copy_file(source, destination)) return false;
    }
    return true;
}

static void count_modifiers(BongoJsonValue *matrix, size_t counts[3]) {
    size_t row_index, row_count; BongoJsonValue *row;
    bongo_json_arr_foreach(matrix, row_index, row_count, row) {
        size_t key_index, key_count; BongoJsonValue *key;
        bongo_json_arr_foreach(row, key_index, key_count, key) {
            int index = (bongo_json_is_int(key) || bongo_json_is_uint(key))
                ? bongo_cat_mver_modifier_index((int)bongo_json_get_int(key)) : -1;
            if (index >= 0) counts[index]++;
        }
    }
}

static bool keyboard_index(BongoJsonValue *matrix, int code, size_t *result) {
    if (!bongo_json_is_arr(matrix)) return false;
    size_t row_index, row_count; BongoJsonValue *row;
    bongo_json_arr_foreach(matrix, row_index, row_count, row) {
        size_t key_index, key_count; BongoJsonValue *key;
        bongo_json_arr_foreach(row, key_index, key_count, key) {
            if ((bongo_json_is_int(key) || bongo_json_is_uint(key)) &&
                bongo_json_get_int(key) == code) {
                *result = row_index;
                return true;
            }
        }
    }
    return false;
}

static bool process_matrix(const BongoCatImportCandidate *candidate, BongoJsonValue *matrix,
    BongoJsonValue *before, BongoJsonValue *after, BongoJsonValue *keyboard_matrix,
    const char *hand_name, const char *key_group, const char *target,
    BongoCatError *error) {
    if (!matrix || bongo_json_is_null(matrix)) return true;
    if (!bongo_json_is_arr(matrix)) return false;
    if (!bongo_json_arr_size(matrix)) return true;
    char resources[BONGO_CAT_PATH_CAP], output_dir[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_path_join(resources, sizeof(resources), target, "resources") ||
        !bongo_cat_path_join(output_dir, sizeof(output_dir), resources, key_group) ||
        !bongo_cat_path_create_directory(output_dir)) return false;
    size_t modifier_total[3] = {0}, modifier_seen[3] = {0};
    count_modifiers(before, modifier_seen);
    memcpy(modifier_total, modifier_seen, sizeof(modifier_total));
    count_modifiers(matrix, modifier_total);
    count_modifiers(after, modifier_total);
    size_t index, count; BongoJsonValue *keys;
    bongo_json_arr_foreach(matrix, index, count, keys) {
        char hand[BONGO_CAT_PATH_CAP], filename[32];
        snprintf(filename, sizeof(filename), "%zu.png", index);
        if (!asset_file(candidate, hand_name, filename, hand, sizeof(hand)) ||
            !bongo_cat_image_info(hand, NULL, NULL)) {
            size_t missing_index, missing_count; BongoJsonValue *missing;
            bongo_json_arr_foreach(keys, missing_index, missing_count, missing) {
                int modifier = (bongo_json_is_int(missing) || bongo_json_is_uint(missing))
                    ? bongo_cat_mver_modifier_index((int)bongo_json_get_int(missing)) : -1;
                if (modifier >= 0) modifier_seen[modifier]++;
            }
            continue;
        }
        size_t key_index, key_count; BongoJsonValue *key;
        bongo_json_arr_foreach(keys, key_index, key_count, key) {
            if (!bongo_json_is_int(key) && !bongo_json_is_uint(key)) {
                bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
                    "Mver input row %zu contains a non-integer key", index);
                return false;
            }
            int code = (int)bongo_json_get_int(key);
            char keyboard[BONGO_CAT_PATH_CAP];
            size_t keyboard_row;
            const char *keyboard_path = NULL;
            if (keyboard_index(keyboard_matrix, code, &keyboard_row)) {
                snprintf(filename, sizeof(filename), "%zu.png", keyboard_row);
                if (asset_file(candidate, "keyboard", filename, keyboard,
                    sizeof(keyboard)) && bongo_cat_image_info(keyboard, NULL, NULL))
                    keyboard_path = keyboard;
            }
            int modifier = bongo_cat_mver_modifier_index(code);
            size_t occurrence = modifier >= 0 ? modifier_seen[modifier]++ : 0;
            KeyNames names = candidate->gamepad_buttons
                ? bongo_cat_mver_gamepad_names(code) : bongo_cat_mver_device_names(code, occurrence,
                    modifier >= 0 ? modifier_total[modifier] : 0);
            if (!names.count) {
                bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
                    "Mver input row %zu uses unsupported key code %d", index, code);
                return false;
            }
            if (!bongo_cat_mver_emit_pair(hand, keyboard_path, output_dir, names, error)) return false;
        }
    }
    return true;
}

bool bongo_cat_import_mver_assets(const BongoCatImportCandidate *candidate,
    const char *target, BongoCatError *error) {
    if (candidate->format != BONGO_CAT_IMPORT_MVER &&
        candidate->format != BONGO_CAT_IMPORT_MVER_PATCH) return true;
    FILE *file = bongo_cat_file_open(candidate->config, "rb");
    BongoJsonDoc *document = file ? bongo_json_read_fp(file,
        BONGO_JSON_READ_JSON5 | BONGO_JSON_READ_ALLOW_INVALID_UNICODE, NULL, NULL) : NULL;
    if (file) fclose(file);
    BongoJsonValue *root = document ? bongo_json_doc_get_root(document) : NULL;
    BongoJsonValue *mode = bongo_json_is_obj(root)
        ? bongo_json_obj_get(root, bongo_cat_mode_name(candidate->mode)) : NULL;
    if (!bongo_json_is_obj(mode)) {
        bongo_json_doc_free(document);
        bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
            "Cannot parse Mver input configuration: %s", candidate->config);
        return false;
    }
    bool ok = false;
    BongoJsonValue *keyboard = bongo_json_obj_get(mode, "keyboard");
    if (candidate->mode == BONGO_CAT_MODE_STANDARD) {
        BongoJsonValue *hand = bongo_json_obj_get(mode, "hand");
        ok = process_matrix(candidate, hand, NULL, NULL,
            keyboard, "hand", "left-keys", target, error);
        if (ok) ok = copy_standard_pointer_assets(candidate, target);
    } else {
        BongoJsonValue *left_keys = bongo_json_obj_get(mode, "lefthand");
        BongoJsonValue *right_keys = bongo_json_obj_get(mode, "righthand");
        ok = process_matrix(candidate, left_keys, NULL, right_keys, keyboard,
            "lefthand", "left-keys", target, error) &&
            process_matrix(candidate, right_keys, left_keys, NULL, keyboard,
                "righthand", "right-keys", target, error);
    }
    bongo_json_doc_free(document);
    if (!ok && error && !error->message[0]) bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
        "Cannot convert Mver input configuration: %s", candidate->config);
    return ok;
}
