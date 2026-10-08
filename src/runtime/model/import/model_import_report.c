#include "model_import.h"
#include "mver/model_import_mver_manifest.h"
#include "bongo_cat/file.h"
#include "bongo_cat/json.h"
#include "bongo_cat/path.h"

#include <stdio.h>
#include <string.h>
#include "bongo_cat/json_dom.h"

#define IMPORT_REPORT ".bongo-cat-import-report.json"

static bool source_asset(const BongoCatImportCandidate *candidate,
    const char *group, size_t index) {
    char name[32], directory[BONGO_CAT_PATH_CAP], path[BONGO_CAT_PATH_CAP];
    snprintf(name, sizeof(name), "%zu.png", index);
    if (candidate->overrides[0] &&
        bongo_cat_path_join(directory, sizeof(directory), candidate->overrides, group) &&
        bongo_cat_path_join(path, sizeof(path), directory, name) &&
        bongo_cat_path_is_file(path)) return true;
    return bongo_cat_path_join(directory, sizeof(directory), candidate->assets, group) &&
        bongo_cat_path_join(path, sizeof(path), directory, name) &&
        bongo_cat_path_is_file(path);
}

static bool source_sound(const BongoCatImportCandidate *candidate, size_t index) {
    static const char *extensions[] = {"wav", "ogg", "flac"};
    char directory[BONGO_CAT_PATH_CAP], path[BONGO_CAT_PATH_CAP], name[32];
    if (!bongo_cat_path_join(directory, sizeof(directory), candidate->assets,
        "sounds")) return false;
    for (size_t i = 0; i < sizeof(extensions) / sizeof(extensions[0]); ++i) {
        snprintf(name, sizeof(name), "%zu.%s", index, extensions[i]);
        if (bongo_cat_path_join(path, sizeof(path), directory, name) &&
            bongo_cat_path_is_file(path)) return true;
    }
    return false;
}

static bool add_stat(BongoJsonMutDoc *output, BongoJsonMutValue *stats,
    BongoJsonValue *mode, const BongoCatImportCandidate *candidate,
    const char *key, const char *directory, bool sound) {
    BongoJsonValue *rows = bongo_json_obj_get(mode, key);
    if (!rows || bongo_json_is_null(rows)) return true;
    if (!bongo_json_is_arr(rows)) return false;
    size_t declared = bongo_json_arr_size(rows), available = 0;
    for (size_t i = 0; i < declared; ++i)
        if (sound ? source_sound(candidate, i) : source_asset(candidate, directory, i))
            available++;
    BongoJsonMutValue *value = bongo_json_mut_obj_add_obj(output, stats, key);
    return value && bongo_json_mut_obj_add_uint(output, value, "declared", declared) &&
        bongo_json_mut_obj_add_uint(output, value, "available", available) &&
        bongo_json_mut_obj_add_uint(output, value, "missing", declared - available);
}

static bool add_degradation(BongoJsonMutDoc *output, BongoJsonMutValue *items,
    const char *field, const char *reason) {
    BongoJsonMutValue *item = bongo_json_mut_arr_add_obj(output, items);
    return item && bongo_json_mut_obj_add_strcpy(output, item, "field", field) &&
        bongo_json_mut_obj_add_strcpy(output, item, "reason", reason);
}

static bool configured(BongoJsonValue *object, const char *key) {
    return bongo_json_is_obj(object) && bongo_json_obj_get(object, key) != NULL;
}

static bool add_degradations(BongoJsonMutDoc *output, BongoJsonMutValue *items,
    BongoJsonValue *config, const BongoCatImportCandidate *candidate) {
    BongoJsonValue *decoration = bongo_json_obj_get(config, "decoration");
    BongoJsonValue *mode = bongo_json_obj_get(config, bongo_cat_mode_name(candidate->mode));
    bool ok = true;
    if (ok && (configured(decoration, "offsetX") || configured(decoration, "offsetY") ||
        configured(decoration, "scalar") || configured(decoration, "hand_offset")))
        ok = add_degradation(output, items, "decoration.sprite_geometry",
            "Mver pixel offsets are tied to its fixed canvas and are not applied to BongoCat layouts");
    if (ok && candidate->mode == BONGO_CAT_MODE_GAMEPAD &&
        (configured(mode, "stick_offset_L") || configured(mode, "stick_offset_R")))
        ok = add_degradation(output, items, "gamepad.stick_offsets",
            "BongoCat uses normalized controller axes rather than Mver sprite offsets");
    return ok;
}

static size_t missing_motion_sounds(const BongoCatImportCandidate *candidate) {
    if (candidate->format == BONGO_CAT_IMPORT_INOCHI2D) return 0;
    char manifest_path[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_path_join(manifest_path, sizeof(manifest_path),
        candidate->directory, candidate->setting)) return 0;
    BongoJsonDoc *document = candidate->format == BONGO_CAT_IMPORT_TAURI
        ? bongo_cat_json_read_file(manifest_path, 0, NULL)
        : bongo_cat_import_mver_manifest_read(manifest_path, NULL);
    BongoJsonValue *refs = document ? bongo_json_obj_get(bongo_json_doc_get_root(document),
        "FileReferences") : NULL;
    BongoJsonValue *motions = bongo_json_obj_get(refs, "Motions");
    size_t missing = 0, group_index, group_count; BongoJsonValue *key, *group;
    bongo_json_obj_foreach(motions, group_index, group_count, key, group) {
        size_t index, count; BongoJsonValue *item;
        bongo_json_arr_foreach(group, index, count, item) {
            const char *sound = bongo_json_get_str(bongo_json_obj_get(item, "Sound"));
            char path[BONGO_CAT_PATH_CAP];
            if (sound && (!bongo_cat_path_join(path, sizeof(path),
                candidate->directory, sound) || !bongo_cat_path_is_file(path))) missing++;
        }
    }
    bongo_json_doc_free(document);
    return missing;
}

static const char *format_name(BongoCatImportFormat format) {
    if (format == BONGO_CAT_IMPORT_INOCHI2D) return "inochi2d";
    if (format == BONGO_CAT_IMPORT_MVER) return "bongo-cat-mver";
    if (format == BONGO_CAT_IMPORT_MVER_PATCH) return "bongo-cat-mver-patch";
    return "tauri-live2d";
}

bool bongo_cat_import_write_report(const BongoCatImportCandidate *candidate,
    const char *target, BongoCatError *error) {
    BongoJsonDoc *source = candidate->config[0] ? bongo_cat_json_read_file(
        candidate->config, BONGO_JSON_READ_JSON5 | BONGO_JSON_READ_ALLOW_INVALID_UNICODE, NULL) : NULL;
    BongoJsonValue *config = source ? bongo_json_doc_get_root(source) : NULL;
    BongoJsonValue *mode = bongo_json_obj_get(config, bongo_cat_mode_name(candidate->mode));
    BongoJsonMutDoc *output = bongo_json_mut_doc_new(NULL);
    BongoJsonMutValue *root = output ? bongo_json_mut_obj(output) : NULL;
    BongoJsonMutValue *stats = root ? bongo_json_mut_obj_add_obj(output, root, "assets") : NULL;
    BongoJsonMutValue *capabilities = root ? bongo_json_mut_obj_add_obj(output, root,
        "capabilities") : NULL;
    BongoJsonMutValue *degraded = root ? bongo_json_mut_obj_add_arr(output, root,
        "degradations") : NULL;
    if (output) bongo_json_mut_doc_set_root(output, root);
    bool mver = (candidate->format == BONGO_CAT_IMPORT_MVER ||
        candidate->format == BONGO_CAT_IMPORT_MVER_PATCH);
    bool ok = root && stats && capabilities && degraded &&
        bongo_json_mut_obj_add_int(output, root, "schemaVersion", 1) &&
        bongo_json_mut_obj_add_strcpy(output, root, "format", format_name(candidate->format)) &&
        bongo_json_mut_obj_add_str(output, root, "runtimeProfile",
            mver ? "mver-0.1.6" : "native") &&
        bongo_json_mut_obj_add_strcpy(output, root, "mode",
            bongo_cat_mode_name(candidate->mode)) &&
        bongo_json_mut_obj_add_bool(output, capabilities, "live2dModel", candidate->format != BONGO_CAT_IMPORT_INOCHI2D) &&
        bongo_json_mut_obj_add_bool(output, capabilities, "inochi2dModel", candidate->format == BONGO_CAT_IMPORT_INOCHI2D) &&
        bongo_json_mut_obj_add_bool(output, capabilities, "previewAssets", true) &&
        bongo_json_mut_obj_add_bool(output, capabilities, "sourceStructurePreserved", true) &&
        bongo_json_mut_obj_add_bool(output, capabilities, "adapterIsolation", true) &&
        bongo_json_mut_obj_add_bool(output, capabilities, "handInputImages", mver) &&
        bongo_json_mut_obj_add_bool(output, capabilities, "keyboardChords", mver) &&
        bongo_json_mut_obj_add_bool(output, capabilities, "gamepadButtons",
            mver && candidate->gamepad_buttons) &&
        bongo_json_mut_obj_add_bool(output, capabilities, "expressionsAndMotions", candidate->format != BONGO_CAT_IMPORT_INOCHI2D) &&
        bongo_json_mut_obj_add_bool(output, capabilities, "shortcutAudio", mver) &&
        bongo_json_mut_obj_add_bool(output, capabilities, "imageEffects", mver) &&
        bongo_json_mut_obj_add_bool(output, capabilities, "mverRuntime", mver) &&
        bongo_json_mut_obj_add_bool(output, capabilities, "tauriAdapter", candidate->format == BONGO_CAT_IMPORT_TAURI) &&
        bongo_json_mut_obj_add_bool(output, capabilities, "mverProjection", mver) &&
        bongo_json_mut_obj_add_bool(output, capabilities, "mverPointerDomain", mver) &&
        bongo_json_mut_obj_add_bool(output, capabilities, "imagePatch",
            candidate->format == BONGO_CAT_IMPORT_MVER_PATCH) &&
        bongo_json_mut_obj_add_uint(output, root, "optionalMissingMotionSounds",
            missing_motion_sounds(candidate));
    if (ok && mver) {
        const char *left = candidate->mode == BONGO_CAT_MODE_STANDARD
            ? "hand" : "lefthand";
        ok = bongo_json_is_obj(config) && bongo_json_is_obj(mode) &&
            add_stat(output, stats, mode, candidate, left, left, false) &&
            (candidate->mode == BONGO_CAT_MODE_STANDARD ||
                add_stat(output, stats, mode, candidate, "righthand", "righthand", false)) &&
            add_stat(output, stats, mode, candidate, "keyboard", "keyboard", false) &&
            add_stat(output, stats, mode, candidate, "face", "face", false) &&
            add_stat(output, stats, mode, candidate, "sounds", "sounds", true) &&
            add_degradations(output, degraded, config, candidate);
    }
    if (ok) ok = bongo_json_mut_obj_add_str(output, root, "status",
        bongo_json_mut_arr_size(degraded) ? "imported-with-documented-degradations" : "imported");
    char path[BONGO_CAT_PATH_CAP];
    if (ok) ok = bongo_cat_path_join(path, sizeof(path), target, IMPORT_REPORT) &&
        bongo_cat_json_write_file(path, output, BONGO_JSON_WRITE_PRETTY, NULL);
    bongo_json_mut_doc_free(output);
    bongo_json_doc_free(source);
    if (!ok) bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
        "Cannot write model compatibility report");
    return ok;
}
