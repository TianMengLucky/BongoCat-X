#include "plugin_worker.h"
#include "bongo_cat/safe_worker.h"
#include <string.h>
static bool invoke(void *context, const void *job, void *error) {
    BongoCatPluginWorker *w = context;
    return w->work(w->context, job, error);
}
bool bongo_cat_plugin_worker_start(BongoCatPluginWorker *w, const char *name,
    BongoCatPluginWork work, void *context, bool poll_idle) {
    if (!w || !work) return false;
    memset(w, 0, sizeof(*w)); atomic_init(&w->changed, false);
    w->work = work; w->context = context;
    w->state_mutex = SDL_CreateMutex();
    if (w->state_mutex) w->core = bongo_safe_worker_new(name, invoke, w, poll_idle,
        sizeof(BongoCatPluginJob), sizeof(BongoCatError));
    if (w->core) return true;
    bongo_cat_plugin_worker_stop(w); return false;
}
bool bongo_cat_plugin_worker_submit(BongoCatPluginWorker *w, const BongoCatPluginJob *job) {
    return w && bongo_safe_worker_submit(w->core, job);
}
bool bongo_cat_plugin_worker_busy(BongoCatPluginWorker *w) {
    return w && bongo_safe_worker_busy(w->core);
}
bool bongo_cat_plugin_worker_current(const BongoCatPluginWorker *w) {
    return w && bongo_safe_worker_current(w->core);
}
bool bongo_cat_plugin_worker_poll(BongoCatPluginWorker *w, bool *failed, BongoCatError *error) {
    return w && bongo_safe_worker_poll(w->core, failed, error);
}
void bongo_cat_plugin_worker_stop(BongoCatPluginWorker *w) {
    if (!w) return;
    bongo_safe_worker_free(w->core);
    SDL_DestroyMutex(w->state_mutex);
    memset(w, 0, sizeof(*w));
}
