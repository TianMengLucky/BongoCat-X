#include "sounds_internal.h"
#include "bongo_cat/json.h"
#include "bongo_cat/path.h"
#include "bongo_cat/file.h"
#include "bongo_cat/sha256.h"
#include "bongo_cat/safe_ffi.h"
#include "bongo_cat/preferences.h"
#include <stdio.h>
#include <string.h>
static bool scene_path(BongoCatApp *app, const char *name, char *out) {
    char directory[BONGO_CAT_PATH_CAP];
    return bongo_cat_path_join(directory, sizeof(directory), app->data_root, "sounds") &&
        bongo_cat_path_create_directory(directory) && bongo_cat_path_join(out, BONGO_CAT_PATH_CAP, directory, name);
}
bool bongo_cat_sounds_key(BongoCatApp *app, uint32_t index, BongoCatSoundKey *key) {
    if (!app || !app->sounds || !key || !app->sounds->api->key(index, key)) return false;
    return memchr(key->id, 0, sizeof(key->id)) && memchr(key->label, 0, sizeof(key->label)) &&
        key->id[0] && key->label[0] && key->row < 6 && key->x >= 0 && key->x <= 23 &&
        key->width > 0 && key->width <= 7 && key->x + key->width <= 23;
}
static int key_index(BongoCatApp *app, const char *name) {
    for (unsigned i = 0; i < app->sounds->api->key_count(); ++i) {
        BongoCatSoundKey key;
        if (bongo_cat_sounds_key(app, i, &key) && !strcmp(key.id, name)) return (int)i;
    }
    return -1;
}
bool bongo_cat_sounds_load_scene(BongoCatApp *app, BongoCatError *error) {
    char path[BONGO_CAT_PATH_CAP];
    if (!scene_path(app, "bindings.json", path)) goto failed;
    if (!bongo_cat_path_is_file(path)) return true;
    uint64_t size = 0;
    if (!bongo_cat_path_file_size(path, &size) || size > 65536) goto failed;
    BongoJsonDoc *doc = bongo_cat_json_read_file(path, 0, NULL);
    BongoJsonValue *root = bongo_json_doc_get_root(doc);
    BongoJsonValue *version = bongo_json_obj_get(root, "version");
    BongoJsonValue *bindings = bongo_json_obj_get(root, "bindings");
    BongoCatSoundScene scene;
    bool ok = doc && bongo_json_is_int(version) && bongo_json_get_int(version) == 1 &&
        bongo_json_is_obj(bindings) && bongo_json_obj_size(bindings) <= BONGO_CAT_SOUNDS_CAP &&
        bongo_cat_sounds_snapshot(app, &scene);
    for (size_t i = 0; ok && i < bongo_json_obj_size(bindings); ++i) {
        const char *key = bongo_json_get_str(bongo_json_obj_key_at(bindings, i));
        int index = key ? key_index(app, key) : -1;
        BongoJsonValue *obj = bongo_json_obj_value_at(bindings, i);
        const char *asset = bongo_json_get_str(bongo_json_obj_get(obj, "asset"));
        const char *name = bongo_json_get_str(bongo_json_obj_get(obj, "name"));
        ok = index >= 0 && asset && name && strlen(asset) < 80 && strlen(name) < 128;
        if (ok) {
            SDL_strlcpy(scene.bindings[index].asset, asset, sizeof(scene.bindings[index].asset));
            SDL_strlcpy(scene.bindings[index].name, name, sizeof(scene.bindings[index].name));
        }
    }
    if (ok) {
        SDL_LockMutex(app->sounds->worker.state_mutex);
        ok = app->sounds->api->restore(app->sounds->instance, &scene) != 0;
        SDL_UnlockMutex(app->sounds->worker.state_mutex);
    }
    bongo_json_doc_free(doc);
    if (ok) return true;
failed:
    bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT, "Cannot load sound bindings; original file is preserved"); return false;
}
bool bongo_cat_sounds_save(BongoCatApp *app, BongoCatError *error) {
    BongoCatSoundScene scene;
    if (!bongo_cat_sounds_snapshot(app, &scene)) return false;
    BongoJsonMutDoc *doc = bongo_json_mut_doc_new(NULL);
    BongoJsonMutValue *root = bongo_json_mut_obj(doc);
    bongo_json_mut_doc_set_root(doc, root);
    BongoJsonMutValue *bindings = bongo_json_mut_obj_add_obj(doc, root, "bindings");
    bool ok = doc && root && bindings && bongo_json_mut_obj_add_int(doc, root, "version", 1);
    for (unsigned i = 0; ok && i < scene.count; ++i) {
        if (!scene.bindings[i].asset[0]) continue;
        BongoCatSoundKey key;
        ok = bongo_cat_sounds_key(app, i, &key);
        BongoJsonMutValue *obj = ok ? bongo_json_mut_obj_add_obj(doc, bindings, key.id) : NULL;
        ok = obj && bongo_json_mut_obj_add_strcpy(doc, obj, "asset", scene.bindings[i].asset) &&
            bongo_json_mut_obj_add_strcpy(doc, obj, "name", scene.bindings[i].name);
    }
    char path[BONGO_CAT_PATH_CAP], temporary[BONGO_CAT_PATH_CAP] = {0};
    ok = ok && scene_path(app, "bindings.json", path) && scene_path(app, "bindings.json.new", temporary) &&
        bongo_cat_json_write_file(temporary, doc, BONGO_JSON_WRITE_PRETTY, NULL) &&
        bongo_cat_file_replace(temporary, path, true);
    if (temporary[0]) (void)bongo_cat_file_remove(temporary);
    bongo_json_mut_doc_free(doc);
    if (!ok) bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Cannot save sound bindings");
    return ok;
}
static bool bind_saved(BongoCatApp *app, unsigned index, const BongoCatSoundBinding *binding, BongoCatError *error) {
    BongoCatSoundScene before;
    if (!bongo_cat_sounds_snapshot(app, &before)) return false;
    SDL_LockMutex(app->sounds->worker.state_mutex);
    bool bound = app->sounds->api->bind(app->sounds->instance, index, binding) != 0;
    SDL_UnlockMutex(app->sounds->worker.state_mutex);
    if (!bound) return false;
    if (!bongo_cat_sounds_save(app, error)) {
        SDL_LockMutex(app->sounds->worker.state_mutex);
        (void)app->sounds->api->restore(app->sounds->instance, &before);
        SDL_UnlockMutex(app->sounds->worker.state_mutex); return false;
    }
    /* Stop the previous clip without touching model or other plugin audio. */
    char path[BONGO_CAT_PATH_CAP];
    if (before.bindings[index].asset[0] && scene_path(app, before.bindings[index].asset, path))
        bongo_cat_audio_stop_path(app->sounds->audio, path);
    atomic_store_explicit(&app->sounds->worker.changed, true, memory_order_release); return true;
}
bool bongo_cat_sounds_clear_now(BongoCatApp *app, uint32_t index, BongoCatError *error) {
    BongoCatSoundBinding empty = {0}; return bind_saved(app, index, &empty, error);
}
bool bongo_cat_sounds_import_audio_now(BongoCatApp *app, const char *key, const char *source, BongoCatError *error) {
    if (!app || !app->sounds || !key || !source) return false;
    int index = key_index(app, key);
    if (index < 0) return false;
    uint64_t size = 0; char digest[65], copied[65], target[BONGO_CAT_PATH_CAP] = {0};
    bool created = false;
    if (!bongo_cat_path_file_size(source, &size) || !size || size > 64u * 1024u * 1024u ||
        bongo_cat_sha256_file(source, digest, error) != BONGO_CAT_OK) goto failed;
    BongoCatSoundBinding binding = {0}; snprintf(binding.asset, sizeof(binding.asset), "%s.snd", digest);
    if (!scene_path(app, binding.asset, target)) goto failed;
    if (!bongo_cat_path_is_file(target)) { created = true; if (!bongo_cat_path_copy_file(source, target)) goto failed; }
    if (!bongo_cat_path_file_size(target, &size) || size > 64u * 1024u * 1024u ||
        bongo_cat_sha256_file(target, copied, error) != BONGO_CAT_OK || strcmp(digest, copied)) goto failed;
    float *samples = NULL; uint64_t frames = 0; uint32_t rate = 0, channels = 0;
    bool valid = bongo_safe_audio_decode_file(target, &samples, &frames, &rate, &channels);
    if (samples) bongo_safe_free_samples(samples, (size_t)frames * channels);
    if (!valid) goto failed;
    const char *name = source;
    for (const char *p = source; *p; ++p) if (*p == '/' || *p == '\\') name = p + 1;
    SDL_utf8strlcpy(binding.name, name, sizeof(binding.name));
    if (!bind_saved(app, (unsigned)index, &binding, error)) goto failed;
    return true;
failed:
    if (created && target[0]) (void)bongo_cat_file_remove(target);
    if (!error || !error->message[0]) bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
        "Cannot import audio; choose WAV, MP3, Ogg Vorbis or FLAC, up to 64 MB and decoder limits");
    return false;
}
