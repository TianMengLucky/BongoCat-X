#include "sounds_internal.h"
#include "bongo_cat/path.h"
#include "bongo_cat/log.h"
static bool play_binding(BongoCatApp *app, const BongoCatSoundBinding *binding, BongoCatError *error) {
    char directory[BONGO_CAT_PATH_CAP], path[BONGO_CAT_PATH_CAP];
    uint64_t size = 0;
    if (!binding->asset[0]) return true;
    if (!bongo_cat_path_join(directory, sizeof(directory), app->data_root, "sounds") ||
        !bongo_cat_path_join(path, sizeof(path), directory, binding->asset) ||
        !bongo_cat_path_file_size(path, &size) || !size || size > 64u * 1024u * 1024u) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Sound file is missing or oversized"); return false;
    }
    return bongo_cat_audio_play_sound(app->sounds->audio, path, true, error) == BONGO_CAT_OK;
}
bool bongo_cat_sounds_preview_now(BongoCatApp *app, uint32_t index, BongoCatError *error) {
    BongoCatSoundScene scene;
    return bongo_cat_sounds_snapshot(app, &scene) && index < scene.count && play_binding(app, &scene.bindings[index], error);
}
void bongo_cat_sounds_input_now(BongoCatApp *app, const BongoCatInputEvent *event, bool play) {
    if (!app || !app->sounds || !event ||
        (event->kind != BONGO_CAT_INPUT_KEY_DOWN && event->kind != BONGO_CAT_INPUT_KEY_UP)) return;
    BongoCatSoundBinding binding;
    if (app->sounds->api->press(app->sounds->instance, event->name,
        event->kind == BONGO_CAT_INPUT_KEY_DOWN ? 1u : 0u, &binding) && play) {
        BongoCatError error = {0};
        if (!play_binding(app, &binding, &error))
            SDL_LogWarn(BONGO_CAT_LOG_INPUT, "[sounds] %s", error.message);
    }
}
