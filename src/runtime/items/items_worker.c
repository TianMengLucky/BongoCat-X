#include "items_internal.h"
#include "bongo_cat/preferences.h"
#include "bongo_cat/i18n.h"
#include "bongo_cat/log.h"
#include "preferences_notice.h"
#include <string.h>
static bool work(void *context, const BongoCatPluginJob *job, BongoCatError *error) {
    BongoCatApp *app = context;
    if (job->kind == 0 || job->kind == 255) return true;
    if (job->kind == 3) {
        bool ok = bongo_cat_items_load_scene(app, error);
        atomic_store_explicit(&app->items->loaded, ok, memory_order_release);
        atomic_store_explicit(&app->items->worker.changed, true, memory_order_release); return ok;
    }
    if (job->kind == 1) return bongo_cat_items_import_image(app, job->path, error);
    if (job->kind == 2) return bongo_cat_items_save(app, error);
    if (job->kind == 254) {
        if (app->items->loaded && app->items->unsaved && !bongo_cat_items_save(app, error))
            SDL_LogError(BONGO_CAT_LOG_LIFECYCLE, "[items] %s", error->message);
        return true;
    }
    return false;
}
bool bongo_cat_items_worker_start(BongoCatApp *app) {
    if (!bongo_cat_plugin_worker_start(&app->items->worker, "bongo-items-worker", work, app, false)) return false;
    BongoCatPluginJob load = {0}; load.kind = 3;
    return bongo_cat_plugin_worker_submit(&app->items->worker, &load);
}
bool bongo_cat_items_queue_import(BongoCatApp *app, const char *path) {
    if (!app || !app->items || !atomic_load_explicit(&app->items->loaded, memory_order_acquire) || !path || strlen(path) >= BONGO_CAT_PATH_CAP) return false;
    BongoCatPluginJob job = {0}; job.kind = 1; job.notify = true;
    SDL_strlcpy(job.path, path, sizeof(job.path));
    return bongo_cat_plugin_worker_submit(&app->items->worker, &job);
}
bool bongo_cat_items_queue_save(BongoCatApp *app) {
    if (!app || !app->items || !atomic_load_explicit(&app->items->loaded, memory_order_acquire)) return false;
    BongoCatPluginJob job = {0}; job.kind = 2; job.notify = true;
    return bongo_cat_plugin_worker_submit(&app->items->worker, &job);
}
bool bongo_cat_items_busy(BongoCatApp *app) {
    return app && app->items && bongo_cat_plugin_worker_busy(&app->items->worker);
}
void bongo_cat_items_tick(BongoCatApp *app) {
    if (!app || !app->items) return;
    if (atomic_exchange_explicit(&app->items->worker.changed, false, memory_order_acq_rel)) {
        bongo_cat_items_publish(app);
    }
    bool failed; BongoCatError error = {0};
    if (bongo_cat_plugin_worker_poll(&app->items->worker, &failed, &error)) {
        bongo_cat_preferences_invalidate(app->preferences);
        (void)bongo_cat_plugin_event(app, app->items->events, app->items->instance,
            BONGO_PLUGIN_EVENT_COMPLETED, !failed);
    }
}
