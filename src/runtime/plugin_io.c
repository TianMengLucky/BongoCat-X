#include "plugin_io.h"
#include "bongo_cat/file.h"
#include "bongo_cat/image.h"
#include "bongo_cat/path.h"
#include "bongo_cat/safe_ffi.h"
#include "bongo_cat/sha256.h"

static void lock(void *context) {
    SDL_LockMutex(((BongoCatPluginIoContext *)context)->mutex);
}
static void unlock(void *context) {
    SDL_UnlockMutex(((BongoCatPluginIoContext *)context)->mutex);
}
static uint32_t digest(const char *path, char *out) {
    BongoCatError error = {0};
    return bongo_cat_sha256_file(path, out, &error) == BONGO_CAT_OK;
}
static uint32_t replace(const char *source, const char *target) {
    return bongo_cat_file_replace(source, target, true);
}
static uint32_t validate(const char *path, uint32_t media) {
    if (media == BONGO_PLUGIN_IO_AUDIO) {
        float *samples = NULL;
        uint64_t frames = 0;
        uint32_t rate = 0, channels = 0;
        bool ok = bongo_safe_audio_decode_file(path, &samples, &frames, &rate, &channels);
        if (samples) bongo_safe_free_samples(samples, (size_t)frames * channels);
        return ok;
    }
    if (media != BONGO_PLUGIN_IO_IMAGE) return 0;
    int width = 0, height = 0;
    if (!bongo_cat_image_info(path, &width, &height) || width <= 0 || height <= 0 ||
        (uint64_t)width * (uint64_t)height > 16u * 1024u * 1024u) return 0;
    BongoCatImage image = {0};
    BongoCatError error = {0};
    bool ok = bongo_cat_image_load(path, &image, &error) == BONGO_CAT_OK;
    bongo_cat_image_free(&image);
    return ok;
}
static uint32_t play(void *context, const char *path) {
    BongoCatError error = {0};
    BongoCatAudio *audio = ((BongoCatPluginIoContext *)context)->audio;
    return audio && bongo_cat_audio_play_sound(audio, path, true, &error) == BONGO_CAT_OK;
}
static void stop(void *context, const char *path) {
    BongoCatAudio *audio = ((BongoCatPluginIoContext *)context)->audio;
    if (audio) bongo_cat_audio_stop_path(audio, path);
}
BongoCatPluginIoHost bongo_cat_plugin_io_host(BongoCatPluginIoContext *context) {
    return (BongoCatPluginIoHost){BONGO_CAT_PLUGIN_IO_ABI, sizeof(BongoCatPluginIoHost),
        context, lock, unlock, digest, replace, validate, play, stop};
}
const BongoCatPluginIo *bongo_cat_plugin_io_query(SDL_SharedObject *library, const char *symbol) {
    union { SDL_FunctionPointer function; BongoCatPluginIoQuery query; } entry;
    entry.function = SDL_LoadFunction(library, symbol);
    const BongoCatPluginIo *api = entry.function ? entry.query(BONGO_CAT_PLUGIN_IO_ABI) : NULL;
    return api && api->abi_version == BONGO_CAT_PLUGIN_IO_ABI &&
        api->struct_size == sizeof(*api) && api->execute ? api : NULL;
}
