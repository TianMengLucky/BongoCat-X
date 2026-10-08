#include "cubism_plugin_services.hpp"
#include "cubism_model.hpp"

#include <cstring>
#include "bongo_cat/json_dom.h"

namespace bongo_cat {
namespace {

bool safe_path_value(BongoJsonValue *value) {
    const char *path = bongo_json_is_str(value) ? bongo_json_get_str(value) : nullptr;
    if (!path || !path[0] || path[0] == '/' || path[0] == '\\' ||
        std::strchr(path, ':')) return false;
    const char *part = path;
    while (*part) {
        while (*part == '/' || *part == '\\') ++part;
        if (part[0] == '.' && part[1] == '.' &&
            (!part[2] || part[2] == '/' || part[2] == '\\')) return false;
        const char *next = std::strpbrk(part, "/\\");
        if (!next) break;
        part = next;
    }
    return true;
}

bool optional_path(BongoJsonValue *object, const char *key) {
    BongoJsonValue *value = bongo_json_obj_get(object, key);
    return !value || (bongo_json_is_str(value) && !bongo_json_get_str(value)[0]) ||
        safe_path_value(value);
}

bool optional_number(BongoJsonValue *object, const char *key) {
    BongoJsonValue *value = bongo_json_obj_get(object, key);
    return !value || bongo_json_is_num(value);
}

bool valid_textures(BongoJsonValue *value) {
    if (!bongo_json_is_arr(value) || bongo_json_arr_size(value) == 0 ||
        bongo_json_arr_size(value) > 256) return false;
    size_t index, count; BongoJsonValue *item;
    bongo_json_arr_foreach(value, index, count, item)
        if (!safe_path_value(item)) return false;
    return true;
}

bool valid_expressions(BongoJsonValue *value) {
    if (!value) return true;
    if (!bongo_json_is_arr(value) || bongo_json_arr_size(value) > 1024) return false;
    size_t index, count; BongoJsonValue *item;
    bongo_json_arr_foreach(value, index, count, item) {
        if (!bongo_json_is_obj(item) || !bongo_json_is_str(bongo_json_obj_get(item, "Name")) ||
            !safe_path_value(bongo_json_obj_get(item, "File"))) return false;
    }
    return true;
}

bool valid_motion(BongoJsonValue *item) {
    return bongo_json_is_obj(item) && safe_path_value(bongo_json_obj_get(item, "File")) &&
        optional_path(item, "Sound") && optional_number(item, "FadeInTime") &&
        optional_number(item, "FadeOutTime");
}

bool valid_motions(BongoJsonValue *value) {
    if (!value) return true;
    if (!bongo_json_is_obj(value) || bongo_json_obj_size(value) > 1024) return false;
    size_t index, maximum; BongoJsonValue *key, *group;
    bongo_json_obj_foreach(value, index, maximum, key, group) {
        if (!bongo_json_is_str(key) || !bongo_json_is_arr(group) ||
            bongo_json_arr_size(group) > 4096) return false;
        size_t item_index, count; BongoJsonValue *item;
        bongo_json_arr_foreach(group, item_index, count, item)
            if (!valid_motion(item)) return false;
    }
    return true;
}

bool valid_groups(BongoJsonValue *value) {
    if (!value) return true;
    if (!bongo_json_is_arr(value) || bongo_json_arr_size(value) > 1024) return false;
    size_t index, count; BongoJsonValue *group;
    bongo_json_arr_foreach(value, index, count, group) {
        BongoJsonValue *ids = bongo_json_obj_get(group, "Ids");
        if (!bongo_json_is_obj(group) || !bongo_json_is_str(bongo_json_obj_get(group, "Target")) ||
            !bongo_json_is_str(bongo_json_obj_get(group, "Name")) || !bongo_json_is_arr(ids) ||
            bongo_json_arr_size(ids) > 4096) return false;
        size_t id_index, id_count; BongoJsonValue *id;
        bongo_json_arr_foreach(ids, id_index, id_count, id)
            if (!bongo_json_is_str(id)) return false;
    }
    return true;
}

bool valid_hit_areas(BongoJsonValue *value) {
    if (!value) return true;
    if (!bongo_json_is_arr(value) || bongo_json_arr_size(value) > 1024) return false;
    size_t index, count; BongoJsonValue *item;
    bongo_json_arr_foreach(value, index, count, item)
        if (!bongo_json_is_obj(item) || !bongo_json_is_str(bongo_json_obj_get(item, "Id")) ||
            !bongo_json_is_str(bongo_json_obj_get(item, "Name"))) return false;
    return true;
}

bool valid_layout(BongoJsonValue *value) {
    if (!value) return true;
    if (!bongo_json_is_obj(value) || bongo_json_obj_size(value) > 64) return false;
    size_t index, maximum; BongoJsonValue *key, *item;
    bongo_json_obj_foreach(value, index, maximum, key, item)
        if (!bongo_json_is_str(key) || !bongo_json_is_num(item)) return false;
    return true;
}

bool valid_references(BongoJsonValue *value) {
    return bongo_json_is_obj(value) && safe_path_value(bongo_json_obj_get(value, "Moc")) &&
        valid_textures(bongo_json_obj_get(value, "Textures")) &&
        optional_path(value, "Physics") && optional_path(value, "Pose") &&
        optional_path(value, "DisplayInfo") && optional_path(value, "UserData") &&
        valid_expressions(bongo_json_obj_get(value, "Expressions")) &&
        valid_motions(bongo_json_obj_get(value, "Motions"));
}

} // namespace

bool validate_model_setting_json(const std::vector<unsigned char> &json,
    const char *setting_file, BongoCatError *error) {
    if (json.empty() || json.size() > 4 * 1024 * 1024) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
            "Model setting is empty or too large: %s", setting_file ? setting_file : "");
        return false;
    }
    BongoJsonReadError parse_error = {};
    BongoJsonDoc *document = bongo_json_read_opts(
        reinterpret_cast<char *>(const_cast<unsigned char *>(json.data())),
        json.size(), 0, nullptr, &parse_error);
    BongoJsonValue *root = document ? bongo_json_doc_get_root(document) : nullptr;
    bool valid = bongo_json_is_obj(root) && bongo_json_is_int(bongo_json_obj_get(root, "Version")) &&
        bongo_json_get_int(bongo_json_obj_get(root, "Version")) == 3 &&
        valid_references(bongo_json_obj_get(root, "FileReferences")) &&
        valid_groups(bongo_json_obj_get(root, "Groups")) &&
        valid_hit_areas(bongo_json_obj_get(root, "HitAreas")) &&
        valid_layout(bongo_json_obj_get(root, "Layout"));
    if (document) bongo_json_doc_free(document);
    if (valid) return true;
    bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
        "Invalid model setting JSON: %s (%s)", setting_file ? setting_file : "",
        parse_error.msg[0] ? parse_error.msg : "unsupported model3 schema");
    return false;
}

} // namespace bongo_cat
