#include "sounds_internal.h"
#include "bongo_cat/log.h"
bool bongo_cat_sounds_preview_now(BongoCatApp *app, uint32_t index, BongoCatError *error) {
    if (bongo_cat_sounds_execute(app, BONGO_PLUGIN_IO_PREVIEW, index, NULL, NULL, false)) return true;
    bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Sound effects plugin preview failed");
    return false;
}
void bongo_cat_sounds_input_now(BongoCatApp *app, const BongoCatInputEvent *event, bool play) {
    if (!event || (event->kind != BONGO_CAT_INPUT_KEY_DOWN && event->kind != BONGO_CAT_INPUT_KEY_UP)) return;
    if (!bongo_cat_sounds_execute(app, BONGO_PLUGIN_IO_INPUT,
        event->kind == BONGO_CAT_INPUT_KEY_DOWN, event->name, NULL, play))
        SDL_LogWarn(BONGO_CAT_LOG_INPUT, "[sounds] Plugin playback failed");
}
