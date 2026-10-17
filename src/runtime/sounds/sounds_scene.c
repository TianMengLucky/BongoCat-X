#include "sounds_internal.h"
#include "bongo_cat/path.h"
#include <string.h>

bool bongo_cat_sounds_key(BongoCatApp *app, uint32_t index, BongoCatSoundKey *key) {
    if (!app || !app->sounds || !key || !app->sounds->api->key(index, key)) return false;
    return memchr(key->id, 0, sizeof(key->id)) && memchr(key->label, 0, sizeof(key->label)) &&
        key->id[0] && key->label[0] && key->row < 6 && key->x >= 0 && key->x <= 23 &&
        key->width > 0 && key->width <= 7 && key->x + key->width <= 23;
}
bool bongo_cat_sounds_execute(BongoCatApp *app, uint32_t operation, uint32_t index,
    const char *key, const char *path, bool play) {
    char directory[BONGO_CAT_PATH_CAP];
    if (!app || !app->sounds || !app->sounds->io ||
        !bongo_cat_path_join(directory, sizeof(directory), app->data_root, "sounds")) return false;
    BongoCatSounds *v = app->sounds;
    BongoCatPluginIoContext context = {v->worker.state_mutex, v->audio};
    BongoCatPluginIoHost host = bongo_cat_plugin_io_host(&context);
    return v->io->execute(v->instance, directory, &host, operation, index, key, path, play) != 0;
}
static bool execute(BongoCatApp *app, uint32_t operation, uint32_t index,
    const char *key, const char *path, BongoCatError *error) {
    if (bongo_cat_sounds_execute(app, operation, index, key, path, false)) {
        atomic_store_explicit(&app->sounds->worker.changed, true, memory_order_release);
        return true;
    }
    bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Sound effects plugin operation failed; saved bindings are preserved");
    return false;
}
bool bongo_cat_sounds_load_scene(BongoCatApp *app, BongoCatError *error) {
    return execute(app, BONGO_PLUGIN_IO_LOAD, 0, NULL, NULL, error);
}
bool bongo_cat_sounds_save(BongoCatApp *app, BongoCatError *error) {
    return execute(app, BONGO_PLUGIN_IO_SAVE, 0, NULL, NULL, error);
}
bool bongo_cat_sounds_clear_now(BongoCatApp *app, uint32_t index, BongoCatError *error) {
    return execute(app, BONGO_PLUGIN_IO_CLEAR, index, NULL, NULL, error);
}
bool bongo_cat_sounds_import_audio_now(BongoCatApp *app, const char *key, const char *path, BongoCatError *error) {
    return execute(app, BONGO_PLUGIN_IO_IMPORT, 0, key, path, error);
}
