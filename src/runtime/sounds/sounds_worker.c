#include "sounds_internal.h"
#include "bongo_cat/preferences.h"
#include "bongo_cat/i18n.h"
#include "preferences_notice.h"
#include <string.h>
static bool work(void *context, const BongoCatPluginJob *job, BongoCatError *error) {
    BongoCatApp *app = context;
    switch (job->kind) {
    case 0: bongo_cat_audio_update(app->sounds->audio); return true;
    case 5: {
        bool ok = bongo_cat_sounds_load_scene(app, error);
        atomic_store_explicit(&app->sounds->loaded, ok, memory_order_release);
        atomic_store_explicit(&app->sounds->worker.changed, true, memory_order_release); return ok;
    }
    case 1:
        if (!bongo_cat_sounds_import_audio_now(app, job->key, job->path, error)) return false;
        for (unsigned i = 0; i < app->sounds->api->key_count(); ++i) {
            BongoCatSoundKey key;
            if (bongo_cat_sounds_key(app, i, &key) && !strcmp(key.id, job->key)) {
                /* Binding is already saved; a device failure is a preview failure. */
                return bongo_cat_sounds_preview_now(app, i, error);
            }
        }
        return true;
    case 2: return bongo_cat_sounds_clear_now(app, job->index, error);
    case 3: return bongo_cat_sounds_preview_now(app, job->index, error);
    case 4: {
        BongoCatInputEvent event = {0};
        event.kind = job->index ? BONGO_CAT_INPUT_KEY_DOWN : BONGO_CAT_INPUT_KEY_UP;
        SDL_strlcpy(event.name, job->key, sizeof(event.name));
        bongo_cat_sounds_input_now(app, &event, job->play); return true;
    }
    case 254: bongo_cat_audio_reset(app->sounds->audio); return true;
    case 255:
        for (unsigned i = 0; i < app->sounds->api->key_count(); ++i) {
            BongoCatSoundKey key; BongoCatSoundBinding binding;
            if (bongo_cat_sounds_key(app, i, &key)) (void)app->sounds->api->press(app->sounds->instance, key.id, 0, &binding);
        }
        return true;
    default: return false;
    }
}
bool bongo_cat_sounds_worker_start(BongoCatApp *app) {
    if (!bongo_cat_plugin_worker_start(&app->sounds->worker, "bongo-sounds-worker", work, app, true)) return false;
    BongoCatPluginJob load = {0}; load.kind = 5;
    return bongo_cat_plugin_worker_submit(&app->sounds->worker, &load);
}
static bool submit(BongoCatApp *app, BongoCatPluginJob *job, BongoCatError *error) {
    if (app && app->sounds && atomic_load_explicit(&app->sounds->loaded, memory_order_acquire) && bongo_cat_plugin_worker_submit(&app->sounds->worker, job)) return true;
    bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Sound effects plugin is inactive or its queue is full"); return false;
}
bool bongo_cat_sounds_import_audio(BongoCatApp *app, const char *key, const char *path, BongoCatError *error) {
    if (!key || !path || strlen(key) >= 32 || strlen(path) >= BONGO_CAT_PATH_CAP) return false;
    BongoCatPluginJob job = {0}; job.kind = 1; job.notify = true;
    SDL_strlcpy(job.key, key, sizeof(job.key)); SDL_strlcpy(job.path, path, sizeof(job.path));
    return submit(app, &job, error);
}
bool bongo_cat_sounds_clear(BongoCatApp *app, uint32_t index, BongoCatError *error) {
    BongoCatPluginJob job = {0}; job.kind = 2; job.index = index; job.notify = true;
    return submit(app, &job, error);
}
bool bongo_cat_sounds_preview(BongoCatApp *app, uint32_t index, BongoCatError *error) {
    BongoCatPluginJob job = {0}; job.kind = 3; job.index = index;
    return submit(app, &job, error);
}
void bongo_cat_sounds_input(BongoCatApp *app, const BongoCatInputEvent *event, bool play) {
    if (!event || (event->kind != BONGO_CAT_INPUT_KEY_DOWN && event->kind != BONGO_CAT_INPUT_KEY_UP)) return;
    BongoCatPluginJob job = {0}; job.kind = 4; job.index = event->kind == BONGO_CAT_INPUT_KEY_DOWN;
    job.play = play; SDL_strlcpy(job.key, event->name, sizeof(job.key)); (void)submit(app, &job, NULL);
}
bool bongo_cat_sounds_busy(BongoCatApp *app) {
    return app && app->sounds && bongo_cat_plugin_worker_busy(&app->sounds->worker);
}
void bongo_cat_sounds_tick(BongoCatApp *app) {
    if (!app || !app->sounds) return;
    if (atomic_exchange_explicit(&app->sounds->worker.changed, false, memory_order_acq_rel))
        bongo_cat_preferences_invalidate(app->preferences);
    bool failed; BongoCatError error = {0};
    if (bongo_cat_plugin_worker_poll(&app->sounds->worker, &failed, &error))
        bongo_cat_preferences_notice_show(app, bongo_cat_i18n_get(app->i18n,
            failed ? "pages.preference.sounds.failed" : "pages.preference.sounds.saved",
            failed ? "Unable to update sound effects" : "Sound effect saved"), failed);
}
