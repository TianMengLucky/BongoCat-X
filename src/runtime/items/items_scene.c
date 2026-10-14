#include "items_internal.h"
#include "bongo_cat/json.h"
#include "bongo_cat/path.h"
#include "bongo_cat/file.h"
#include "bongo_cat/sha256.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static bool scene_path(BongoCatApp *app, const char *name, char *out) {
    char directory[BONGO_CAT_PATH_CAP];
    return bongo_cat_path_join(directory, sizeof(directory), app->data_root, "items") &&
        bongo_cat_path_create_directory(directory) && bongo_cat_path_join(out, BONGO_CAT_PATH_CAP, directory, name);
}
static bool read_number(BongoJsonValue *obj, const char *key, float *out) {
    BongoJsonValue *v = bongo_json_obj_get(obj, key);
    if (!bongo_json_is_num(v)) return false;
    double n = bongo_json_get_num(v);
    if (!isfinite(n) || n < -10000 || n > 10000) return false;
    *out = (float)n; return true;
}
bool bongo_cat_items_load_scene(BongoCatApp *app, BongoCatError *error) {
    char path[BONGO_CAT_PATH_CAP];
    if (!scene_path(app, "scene.json", path)) goto failed;
    if (!bongo_cat_path_is_file(path)) return true;
    uint64_t size = 0;
    if (!bongo_cat_path_file_size(path, &size) || size > 65536) goto failed;
    BongoJsonDoc *doc = bongo_cat_json_read_file(path, 0, NULL);
    BongoJsonValue *root = bongo_json_doc_get_root(doc);
    BongoJsonValue *version = bongo_json_obj_get(root, "version");
    BongoJsonValue *array = bongo_json_obj_get(root, "items");
    BongoCatItemScene scene = {0}; scene.struct_size = sizeof(scene);
    bool ok = doc && bongo_json_is_int(version) && bongo_json_get_int(version) == 1 &&
        bongo_json_is_arr(array) && bongo_json_arr_size(array) <= BONGO_CAT_ITEMS_CAP;
    if (ok) scene.count = (uint32_t)bongo_json_arr_size(array);
    for (uint32_t i = 0; ok && i < scene.count; ++i) {
        BongoJsonValue *obj = bongo_json_arr_get(array, i);
        BongoCatItem *v = &scene.items[i];
        const char *asset = bongo_json_get_str(bongo_json_obj_get(obj, "asset"));
        const char *name = bongo_json_get_str(bongo_json_obj_get(obj, "name"));
        float layer = 0;
        ok = asset && name && strlen(asset) < sizeof(v->asset) && strlen(name) < sizeof(v->name) &&
            read_number(obj, "x", &v->x) && read_number(obj, "y", &v->y) &&
            read_number(obj, "scale", &v->scale) && read_number(obj, "rotation", &v->rotation) &&
            read_number(obj, "opacity", &v->opacity) && read_number(obj, "layer", &layer) &&
            layer >= -30 && layer <= 30 && floorf(layer) == layer;
        const char *keys[] = {"visible", "locked", "followModel"};
        uint32_t *flags[] = {&v->visible, &v->locked, &v->follow_model};
        for (unsigned j = 0; ok && j < 3; ++j) {
            BongoJsonValue *flag = bongo_json_obj_get(obj, keys[j]);
            ok = bongo_json_is_bool(flag);
            *flags[j] = bongo_json_get_bool(flag) ? 1u : 0u;
        }
        if (ok) {
            memcpy(v->asset, asset, strlen(asset) + 1); memcpy(v->name, name, strlen(name) + 1);
            v->layer = (int32_t)layer;
        }
    }
    if (ok) {
        SDL_LockMutex(app->items->worker.state_mutex);
        ok = app->items->api->restore(app->items->instance, &scene) != 0;
        SDL_UnlockMutex(app->items->worker.state_mutex);
    }
    bongo_json_doc_free(doc);
    if (ok) return true;
failed:
    bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT, "Cannot load items scene; scene.json is preserved");
    return false;
}
bool bongo_cat_items_save(BongoCatApp *app, BongoCatError *error) {
    BongoCatItemScene scene;
    if (!bongo_cat_items_snapshot(app, &scene)) return false;
    BongoJsonMutDoc *doc = bongo_json_mut_doc_new(NULL);
    BongoJsonMutValue *root = bongo_json_mut_obj(doc);
    bongo_json_mut_doc_set_root(doc, root);
    BongoJsonMutValue *array = bongo_json_mut_obj_add_arr(doc, root, "items");
    bool ok = doc && root && array && bongo_json_mut_obj_add_int(doc, root, "version", 1);
    for (uint32_t i = 0; ok && i < scene.count; ++i) {
        const BongoCatItem *v = &scene.items[i];
        BongoJsonMutValue *obj = bongo_json_mut_arr_add_obj(doc, array);
        ok = obj && bongo_json_mut_obj_add_strcpy(doc, obj, "asset", v->asset) &&
            bongo_json_mut_obj_add_strcpy(doc, obj, "name", v->name) &&
            bongo_json_mut_obj_add_real(doc, obj, "x", v->x) &&
            bongo_json_mut_obj_add_real(doc, obj, "y", v->y) &&
            bongo_json_mut_obj_add_real(doc, obj, "scale", v->scale) &&
            bongo_json_mut_obj_add_real(doc, obj, "rotation", v->rotation) &&
            bongo_json_mut_obj_add_real(doc, obj, "opacity", v->opacity) &&
            bongo_json_mut_obj_add_int(doc, obj, "layer", v->layer) &&
            bongo_json_mut_obj_add_bool(doc, obj, "visible", v->visible != 0) &&
            bongo_json_mut_obj_add_bool(doc, obj, "locked", v->locked != 0) &&
            bongo_json_mut_obj_add_bool(doc, obj, "followModel", v->follow_model != 0);
    }
    char path[BONGO_CAT_PATH_CAP], temporary[BONGO_CAT_PATH_CAP] = {0};
    ok = ok && scene_path(app, "scene.json", path) && scene_path(app, "scene.json.new", temporary) &&
        bongo_cat_json_write_file(temporary, doc, BONGO_JSON_WRITE_PRETTY, NULL) &&
        bongo_cat_file_replace(temporary, path, true);
    bongo_json_mut_doc_free(doc);
    if (temporary[0]) (void)bongo_cat_file_remove(temporary);
    if (ok) app->items->unsaved = false;
    else bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Cannot save items scene");
    return ok;
}
bool bongo_cat_items_import_image(BongoCatApp *app, const char *source, BongoCatError *error) {
    BongoCatItemScene scene;
    if (!source || !bongo_cat_items_snapshot(app, &scene) || scene.count >= BONGO_CAT_ITEMS_CAP) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_ARGUMENT, "Items plugin is inactive or the scene has 16 items"); return false;
    }
    uint64_t size = 0; int width = 0, height = 0;
    char digest[65], target[BONGO_CAT_PATH_CAP] = {0};
    BongoCatImage image = {0}; bool created = false;
    if (!bongo_cat_path_file_size(source, &size) || !size || size > 64u * 1024u * 1024u ||
        !bongo_cat_image_info(source, &width, &height) || width <= 0 || height <= 0 ||
        (uint64_t)width * height > 16777216 ||
        bongo_cat_sha256_file(source, digest, error) != BONGO_CAT_OK) goto failed;
    BongoCatItem item = {0};
    snprintf(item.asset, sizeof(item.asset), "%s.img", digest);
    if (!scene_path(app, item.asset, target)) goto failed;
    if (!bongo_cat_path_is_file(target)) {
        created = true;
        if (!bongo_cat_path_copy_file(source, target)) goto failed;
    }
    /* Recheck the managed bytes: source may change while being copied. */
    char copied_digest[65];
    if (!bongo_cat_path_file_size(target, &size) || size > 64u * 1024u * 1024u ||
        bongo_cat_sha256_file(target, copied_digest, error) != BONGO_CAT_OK || strcmp(digest, copied_digest) ||
        !bongo_cat_image_info(target, &width, &height) || width <= 0 || height <= 0 ||
        (uint64_t)width * height > 16777216 ||
        bongo_cat_image_load(target, &image, error) != BONGO_CAT_OK) goto failed;
    bongo_cat_image_free(&image);
    /* Keep a bounded UTF-8 filename; do not split a multi-byte character. */
    const char *name = source;
    for (const char *p = source; *p; ++p) if (*p == '/' || *p == '\\') name = p + 1;
    size_t length = strlen(name);
    if (length >= sizeof(item.name)) {
        length = sizeof(item.name) - 1;
        while (length && ((unsigned char)name[length] & 0xc0) == 0x80) --length;
    }
    memcpy(item.name, name, length); item.name[length] = 0;
    item.x = item.y = 50; item.scale = 25; item.opacity = 100;
    item.visible = item.follow_model = 1;
    SDL_LockMutex(app->items->worker.state_mutex);
    bool added = app->items->api->add(app->items->instance, &item) != 0;
    SDL_UnlockMutex(app->items->worker.state_mutex);
    if (!added) goto failed;
    bongo_cat_items_changed(app);
    if (bongo_cat_items_save(app, error)) return true;
    /* Failed import is transactional: restore the previous scene. */
    SDL_LockMutex(app->items->worker.state_mutex);
    (void)app->items->api->restore(app->items->instance, &scene);
    SDL_UnlockMutex(app->items->worker.state_mutex);
    bongo_cat_items_changed(app);
failed:
    bongo_cat_image_free(&image);
    if (created && target[0]) (void)bongo_cat_file_remove(target);
    if (!error || !error->message[0]) bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
        "Cannot import image (maximum 64 MB / 16 megapixels)");
    return false;
}
