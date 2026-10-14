#include "plugin_worker.h"
#include <string.h>
static int SDLCALL run(void *userdata) {
    BongoCatPluginWorker *w = userdata;
    for (;;) {
        BongoCatPluginJob job = {0};
        SDL_LockMutex(w->mutex);
        while (!w->count && !w->overflow && !w->stopping) {
            if (w->poll_idle) { SDL_WaitConditionTimeout(w->wake, w->mutex, 100); break; }
            SDL_WaitCondition(w->wake, w->mutex);
        }
        if (w->stopping && !w->count) {
            SDL_UnlockMutex(w->mutex); BongoCatPluginJob close = {0}; close.kind = 254;
            BongoCatError error = {0}; (void)w->work(w->context, &close, &error); break;
        }
        bool overflow = w->overflow && !w->count;
        if (overflow) w->overflow = false;
        if (w->count) {
            job = w->jobs[w->head]; w->head = (w->head + 1) % BONGO_CAT_PLUGIN_JOBS; --w->count;
        }
        w->running = job.kind != 0 || overflow; SDL_UnlockMutex(w->mutex);
        BongoCatError error = {0};
        if (overflow) { BongoCatPluginJob reset = {0}; reset.kind = 255; (void)w->work(w->context, &reset, &error); }
        bool ok = w->work(w->context, &job, &error);
        SDL_LockMutex(w->mutex);
        if (job.notify || (!ok && job.kind)) {
            ++w->notices;
            if (!ok) { w->failed = true; w->error = error; }
        }
        w->running = false; SDL_UnlockMutex(w->mutex);
    }
    return 0;
}
bool bongo_cat_plugin_worker_start(BongoCatPluginWorker *w, const char *name,
    BongoCatPluginWork work, void *context, bool poll_idle) {
    memset(w, 0, sizeof(*w)); atomic_init(&w->changed, false);
    w->work = work; w->context = context; w->poll_idle = poll_idle;
    w->mutex = SDL_CreateMutex(); w->state_mutex = SDL_CreateMutex();
    w->wake = SDL_CreateCondition(); w->jobs = SDL_calloc(BONGO_CAT_PLUGIN_JOBS, sizeof(*w->jobs));
    if (w->mutex && w->state_mutex && w->wake && w->jobs)
        w->thread = SDL_CreateThread(run, name, w);
    if (w->thread) return true;
    bongo_cat_plugin_worker_stop(w); return false;
}
bool bongo_cat_plugin_worker_submit(BongoCatPluginWorker *w, const BongoCatPluginJob *job) {
    if (!w || !w->thread || !job) return false;
    SDL_LockMutex(w->mutex);
    bool ok = !w->stopping && w->count < BONGO_CAT_PLUGIN_JOBS;
    if (ok) { w->jobs[(w->head + w->count) % BONGO_CAT_PLUGIN_JOBS] = *job; ++w->count; }
    else if (job->kind == 4) w->overflow = true;
    SDL_SignalCondition(w->wake); SDL_UnlockMutex(w->mutex); return ok;
}
bool bongo_cat_plugin_worker_busy(BongoCatPluginWorker *w) {
    if (!w || !w->thread) return false;
    SDL_LockMutex(w->mutex); bool busy = w->running || w->count != 0 || w->overflow; SDL_UnlockMutex(w->mutex); return busy;
}
bool bongo_cat_plugin_worker_poll(BongoCatPluginWorker *w, bool *failed, BongoCatError *error) {
    if (!w || !w->thread) return false;
    SDL_LockMutex(w->mutex); bool ready = w->notices != 0;
    *failed = w->failed; *error = w->error; w->notices = 0; w->failed = false;
    SDL_UnlockMutex(w->mutex); return ready;
}
void bongo_cat_plugin_worker_stop(BongoCatPluginWorker *w) {
    if (!w) return;
    if (w->thread) {
        SDL_LockMutex(w->mutex); w->stopping = true;
        SDL_BroadcastCondition(w->wake); SDL_UnlockMutex(w->mutex);
        SDL_WaitThread(w->thread, NULL);
    }
    SDL_DestroyCondition(w->wake); SDL_DestroyMutex(w->state_mutex); SDL_DestroyMutex(w->mutex);
    SDL_free(w->jobs); memset(w, 0, sizeof(*w));
}
