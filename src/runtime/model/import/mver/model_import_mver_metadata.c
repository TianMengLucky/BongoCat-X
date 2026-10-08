#include "model_import.h"
#include "model_import_mver_internal.h"
#include "runtime.h"
#include "../../mver/mver_render.h"
#include "bongo_cat/file.h"
#include "bongo_cat/image.h"
#include "bongo_cat/json.h"
#include "bongo_cat/path.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bongo_cat/json_dom.h"

static const char *format_name(BongoCatImportFormat format) {
    if (format == BONGO_CAT_IMPORT_INOCHI2D) return "inochi2d";
    if (format == BONGO_CAT_IMPORT_MVER) return "bongo-cat-mver";
    if (format == BONGO_CAT_IMPORT_MVER_PATCH) return "bongo-cat-mver-patch";
    return "tauri-live2d";
}

static double number_or(BongoJsonValue *value, double fallback) {
    return bongo_json_is_num(value) ? bongo_json_get_num(value) : fallback;
}

static bool usable_pointer_image(const char *path) {
    int width = 0, height = 0;
    return bongo_cat_path_is_file(path) &&
        bongo_cat_image_info(path, &width, &height) &&
        (width > 1 || height > 1);
}

static bool pointer_asset(const BongoCatImportCandidate *candidate,
    const char *name) {
    char path[BONGO_CAT_PATH_CAP];
    if (candidate->overrides[0] && bongo_cat_path_join(path, sizeof(path),
            candidate->overrides, name) && bongo_cat_path_is_file(path))
        return usable_pointer_image(path);
    return bongo_cat_path_join(path, sizeof(path), candidate->assets, name) &&
        usable_pointer_image(path);
}

static bool add_standard_pointer(BongoJsonMutDoc *output, BongoJsonMutValue *root,
    BongoJsonValue *config, const BongoCatImportCandidate *candidate) {
    if (candidate->mode != BONGO_CAT_MODE_STANDARD) return true;
    BongoJsonValue *decoration = bongo_json_obj_get(config, "decoration");
    BongoJsonValue *standard = bongo_json_obj_get(config, "standard");
    BongoJsonValue *mouse_value = bongo_json_obj_get(standard, "mouse");
    bool mouse = bongo_json_is_bool(mouse_value) && bongo_json_get_bool(mouse_value);
    BongoJsonValue *live2d_value = bongo_json_obj_get(standard, "l2d");
    bool live2d = !bongo_json_is_bool(live2d_value) || bongo_json_get_bool(live2d_value);
    size_t device_index = mouse ? 0 : 1;
    BongoJsonValue *offset_x = bongo_json_obj_get(decoration, "offsetX");
    BongoJsonValue *offset_y = bongo_json_obj_get(decoration, "offsetY");
    BongoJsonValue *scale = bongo_json_obj_get(decoration, "scalar");
    BongoJsonValue *hand_offset = bongo_json_obj_get(
        live2d ? standard : decoration, "hand_offset");
    BongoJsonValue *line = bongo_json_obj_get(decoration, "armLineColor");
    BongoJsonValue *left_handed = bongo_json_obj_get(decoration, "leftHanded");
    const char *device = mouse ? "resources/mver-pointer/mouse.png" :
        "resources/mver-pointer/tablet.png";
    const char *left = mouse ? "resources/mver-pointer/mouse_left.png" :
        "resources/mver-pointer/tablet_left.png";
    const char *right = mouse ? "resources/mver-pointer/mouse_right.png" :
        "resources/mver-pointer/tablet_right.png";
    const char *side = mouse ? "resources/mver-pointer/mouse_side.png" : "";
    /* Mver 0.1.6's l2d switch replaces the sprite renderer. The older
       standalone mode 98 instead draws a sprite pointer over Live2D. */
    bool sprite_pointer = !bongo_json_is_true(live2d_value) ||
        bongo_json_get_int(bongo_json_obj_get(config, "mode")) == 98;
    bool enabled = sprite_pointer && pointer_asset(candidate, "arm.png") &&
        pointer_asset(candidate, mouse ? "mouse.png" : "tablet.png");
    BongoJsonMutValue *pointer = bongo_json_mut_obj_add_obj(output, root, "standardPointer");
    return pointer &&
        bongo_json_mut_obj_add_bool(output, pointer, "enabled", enabled) &&
        bongo_json_mut_obj_add_bool(output, pointer, "mouse", mouse) &&
        bongo_json_mut_obj_add_bool(output, pointer, "leftHanded",
            bongo_json_is_bool(left_handed) && bongo_json_get_bool(left_handed)) &&
        bongo_json_mut_obj_add_str(output, pointer, "arm",
            "resources/mver-pointer/arm.png") &&
        bongo_json_mut_obj_add_strcpy(output, pointer, "device", device) &&
        bongo_json_mut_obj_add_strcpy(output, pointer, "left", left) &&
        bongo_json_mut_obj_add_strcpy(output, pointer, "right", right) &&
        bongo_json_mut_obj_add_strcpy(output, pointer, "side", side) &&
        bongo_json_mut_obj_add_real(output, pointer, "offsetX",
            number_or(bongo_json_arr_get(offset_x, device_index), 0.0)) &&
        bongo_json_mut_obj_add_real(output, pointer, "offsetY",
            number_or(bongo_json_arr_get(offset_y, device_index), 0.0)) &&
        bongo_json_mut_obj_add_real(output, pointer, "scale",
            number_or(bongo_json_arr_get(scale, device_index), 1.0)) &&
        bongo_json_mut_obj_add_real(output, pointer, "handOffsetX",
            number_or(bongo_json_arr_get(hand_offset, 0), 0.0)) &&
        bongo_json_mut_obj_add_real(output, pointer, "handOffsetY",
            number_or(bongo_json_arr_get(hand_offset, 1), 0.0)) &&
        bongo_json_mut_obj_add_int(output, pointer, "lineRed",
            (int)number_or(bongo_json_arr_get(line, 0), 0.0)) &&
        bongo_json_mut_obj_add_int(output, pointer, "lineGreen",
            (int)number_or(bongo_json_arr_get(line, 1), 0.0)) &&
        bongo_json_mut_obj_add_int(output, pointer, "lineBlue",
            (int)number_or(bongo_json_arr_get(line, 2), 0.0));
}

static bool add_render_profile(BongoJsonMutDoc *output, BongoJsonMutValue *root,
    BongoJsonValue *config, const BongoCatImportCandidate *candidate) {
    BongoCatModelRuntimeRenderOptions options;
    bongo_cat_mver_render_options(config, &options);
    BongoJsonMutValue *render = bongo_json_mut_obj_add_obj(output, root, "render");
    return render &&
        bongo_json_mut_obj_add_str(output, render, "profile", "mver-0.1.6") &&
        bongo_json_mut_obj_add_bool(output, render, "autoFrame",
            options.auto_frame) &&
        bongo_json_mut_obj_add_real(output, render, "projectionScale", options.projection_scale) &&
        bongo_json_mut_obj_add_real(output, render, "offsetX", options.offset_x) &&
        bongo_json_mut_obj_add_real(output, render, "offsetY", options.offset_y) &&
        bongo_json_mut_obj_add_int(output, render, "referenceWidth", options.reference_width) &&
        bongo_json_mut_obj_add_int(output, render, "referenceHeight", options.reference_height) &&
        bongo_json_mut_obj_add_bool(output, render, "mirror", options.source_mirror) &&
        bongo_json_mut_obj_add_bool(output, render, "pointerLeftHanded", options.pointer_left_handed) &&
        bongo_json_mut_obj_add_bool(output, render, "mouseForceMove", options.mouse_force_move) &&
        bongo_json_mut_obj_add_real(output, render, "mouseSpeed", options.mouse_speed) &&
        bongo_json_mut_obj_add_bool(output, render, "customPointerBounds", options.custom_pointer_bounds) &&
        bongo_json_mut_obj_add_int(output, render, "pointerLeft", options.pointer_left) &&
        bongo_json_mut_obj_add_int(output, render, "pointerTop", options.pointer_top) &&
        bongo_json_mut_obj_add_int(output, render, "pointerRight", options.pointer_right) &&
        bongo_json_mut_obj_add_int(output, render, "pointerBottom", options.pointer_bottom) &&
        add_standard_pointer(output, root, config, candidate);
}

static bool add_native_render(BongoJsonMutDoc *output, BongoJsonMutValue *root) {
    BongoJsonMutValue *render = bongo_json_mut_obj_add_obj(output, root, "render");
    return render && bongo_json_mut_obj_add_str(output, render, "profile", "native");
}

bool bongo_cat_import_adapter_metadata(const BongoCatImportCandidate *candidate,
    const char *target, BongoCatError *error) {
    bool mver = (candidate->format == BONGO_CAT_IMPORT_MVER ||
        candidate->format == BONGO_CAT_IMPORT_MVER_PATCH);
    BongoJsonDoc *source = mver ? bongo_cat_json_read_file(candidate->config,
        BONGO_JSON_READ_JSON5 | BONGO_JSON_READ_ALLOW_INVALID_UNICODE, NULL) : NULL;
    BongoJsonValue *config = source ? bongo_json_doc_get_root(source) : NULL;
    BongoJsonValue *mode = bongo_json_obj_get(config, bongo_cat_mode_name(candidate->mode));
    BongoCatMverLabels *labels = mver ? calloc(1, sizeof(*labels)) : NULL;
    if (mver && !labels) {
        bongo_json_doc_free(source);
        bongo_cat_error_set(error, BONGO_CAT_ERROR_MEMORY, "Cannot allocate Mver labels");
        return false;
    }
    if (mver) bongo_cat_mver_labels_load(candidate->config,
        bongo_cat_mode_name(candidate->mode), labels);
    BongoJsonMutDoc *output = bongo_json_mut_doc_new(NULL);
    BongoJsonMutValue *root = output ? bongo_json_mut_obj(output) : NULL;
    BongoJsonMutValue *items = root ? bongo_json_mut_obj_add_arr(output, root, "bindings") : NULL;
    if (output) bongo_json_mut_doc_set_root(output, root);
    bool ok = items &&
        bongo_json_mut_obj_add_int(output, root, "schemaVersion",
            BONGO_CAT_MODEL_ADAPTER_SCHEMA) &&
        bongo_json_mut_obj_add_str(output, root, "kind", "bongo-cat-runtime-adapter") &&
        bongo_json_mut_obj_add_strcpy(output, root, "sourceFormat",
            format_name(candidate->format));
    if (ok && mver) ok = bongo_json_is_obj(mode) &&
        add_render_profile(output, root, config, candidate) &&
        bongo_cat_mver_add_behaviors(output, items, config, candidate, labels, error) &&
        bongo_cat_mver_add_audio(output, items, config, bongo_json_obj_get(mode, "sounds"),
            candidate, labels, target) &&
        bongo_cat_mver_effects(output, items, config, mode, candidate, target);
    else if (ok) ok = add_native_render(output, root);
    if (ok && mver) {
        /* Adapter data describes assets/rendering only. Mver config owns keys. */
        size_t index, count; BongoJsonMutValue *item;
        bongo_json_mut_arr_foreach(items, index, count, item)
            bongo_json_mut_obj_remove_key(item, "shortcut");
    }
    char path[BONGO_CAT_PATH_CAP];
    if (ok) ok = bongo_cat_path_join(path, sizeof(path), target,
        BONGO_CAT_MODEL_ADAPTER_FILE) &&
        bongo_cat_json_write_file(path, output, BONGO_JSON_WRITE_PRETTY, NULL);
    bongo_json_mut_doc_free(output);
    bongo_json_doc_free(source);
    bongo_cat_mver_labels_clear(labels); free(labels);
    if (!ok && error && !error->message[0])
        bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
            "Cannot create runtime adapter metadata: %s", candidate->config);
    return ok;
}
