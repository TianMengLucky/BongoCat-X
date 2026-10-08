#include "model_import_manifest.h"
#include "runtime.h"
#include "bongo_cat/file.h"
#include "bongo_cat/image.h"
#include "bongo_cat/path.h"

#include <stdio.h>
#include <string.h>
#include "bongo_cat/json_dom.h"

static bool safe_reference(const char *value) {
    if (!value || !value[0] || value[0] == '/' || value[0] == '\\' ||
        strchr(value, ':')) return false;
    const char *part = value;
    while (*part) {
        while (*part == '/' || *part == '\\') part++;
        if (part[0] == '.' && part[1] == '.' &&
            (!part[2] || part[2] == '/' || part[2] == '\\')) return false;
        part = strpbrk(part, "/\\");
        if (!part) break;
    }
    return true;
}
static bool referenced_file(const char *root, const char *relative) {
    char path[BONGO_CAT_PATH_CAP];
    return safe_reference(relative) &&
        bongo_cat_path_join(path, sizeof(path), root, relative) &&
        bongo_cat_path_is_file(path);
}

static bool referenced_texture(const char *root, const char *relative) {
    char path[BONGO_CAT_PATH_CAP];
    if (!safe_reference(relative) ||
        !bongo_cat_path_join(path, sizeof(path), root, relative)) return false;
    return bongo_cat_image_info(path, NULL, NULL);
}

static bool optional_reference(const char *root, BongoJsonValue *refs,
    const char *name, bool allow_missing) {
    BongoJsonValue *value = bongo_json_obj_get(refs, name);
    if (!value) return true;
    const char *relative = bongo_json_get_str(value);
    return relative && (allow_missing ? (!relative[0] || safe_reference(relative))
        : referenced_file(root, relative));
}

static bool behavior_references(const char *root, BongoJsonValue *refs,
    bool allow_missing) {
    BongoJsonValue *expressions = bongo_json_obj_get(refs, "Expressions");
    if (expressions && !bongo_json_is_arr(expressions)) return false;
    size_t index, count; BongoJsonValue *item;
    bongo_json_arr_foreach(expressions, index, count, item) {
        const char *file = bongo_json_get_str(bongo_json_obj_get(item, "File"));
        if (!(allow_missing ? safe_reference(file) : referenced_file(root, file)))
            return false;
    }
    BongoJsonValue *motions = bongo_json_obj_get(refs, "Motions");
    if (motions && !bongo_json_is_obj(motions)) return false;
    size_t group_index, group_count; BongoJsonValue *key, *group;
    bongo_json_obj_foreach(motions, group_index, group_count, key, group) {
        (void)key;
        if (!bongo_json_is_arr(group)) return false;
        bongo_json_arr_foreach(group, index, count, item) {
            const char *file = bongo_json_get_str(bongo_json_obj_get(item, "File"));
            if (!(allow_missing ? safe_reference(file) : referenced_file(root, file)))
                return false;
            const char *sound = bongo_json_get_str(bongo_json_obj_get(item, "Sound"));
            if (sound && !(allow_missing && !sound[0]) && !safe_reference(sound))
                return false;
        }
    }
    return true;
}

bool bongo_cat_import_manifest_document_valid(const char *root,
    BongoJsonDoc *document, bool allow_missing_optional) {
    BongoJsonValue *manifest = document ? bongo_json_doc_get_root(document) : NULL;
    BongoJsonValue *refs = bongo_json_is_obj(manifest)
        ? bongo_json_obj_get(manifest, "FileReferences") : NULL;
    const char *moc = bongo_json_get_str(bongo_json_obj_get(refs, "Moc"));
    BongoJsonValue *textures = bongo_json_obj_get(refs, "Textures");
    bool valid = bongo_json_get_int(bongo_json_obj_get(manifest, "Version")) == 3 &&
        bongo_json_is_obj(refs) && referenced_file(root, moc) && bongo_json_is_arr(textures) &&
        bongo_json_arr_size(textures) > 0;
    size_t index, maximum; BongoJsonValue *texture;
    bongo_json_arr_foreach(textures, index, maximum, texture)
        valid = valid && referenced_texture(root, bongo_json_get_str(texture));
    valid = valid && optional_reference(root, refs, "Physics", allow_missing_optional) &&
        optional_reference(root, refs, "Pose", allow_missing_optional) &&
        optional_reference(root, refs, "DisplayInfo", allow_missing_optional) &&
        (!allow_missing_optional || optional_reference(root, refs, "UserData", true)) &&
        behavior_references(root, refs, allow_missing_optional);
    return valid;
}

bool bongo_cat_import_manifest_valid(const char *root, const char *setting,
    BongoCatError *error) {
    char path[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_path_join(path, sizeof(path), root, setting)) return false;
    FILE *file = bongo_cat_file_open(path, "rb");
    BongoJsonDoc *document = file ? bongo_json_read_fp(file, 0, NULL, NULL) : NULL;
    if (file) fclose(file);
    bool valid = bongo_cat_import_manifest_document_valid(root, document, false);
    bongo_json_doc_free(document);
    if (!valid && error) bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
        "Model manifest or referenced assets are invalid: %s", path);
    return valid;
}
