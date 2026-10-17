#ifndef BONGO_CAT_PLUGIN_WORKER_H
#define BONGO_CAT_PLUGIN_WORKER_H
#include "bongo_cat/common.h"
#include <SDL3/SDL.h>
#include <stdatomic.h>
#define BONGO_CAT_PLUGIN_JOBS 256u
typedef struct BongoCatPluginJob {
    unsigned kind, index;
    bool notify, play;
    char key[32], path[BONGO_CAT_PATH_CAP];
} BongoCatPluginJob;
/* Every instance creates its own thread. Never shared with another plugin. */
typedef bool (*BongoCatPluginWork)(void *context, const BongoCatPluginJob *job, BongoCatError *error);
typedef struct BongoCatPluginWorker {
    void *core;
    SDL_Mutex *state_mutex; /* Guards C plugin/UI snapshots, never the Rust queue. */
    BongoCatPluginWork work;
    void *context;
    atomic_bool changed;
} BongoCatPluginWorker;
/* poll_idle delivers kind 0 every 100 ms for audio maintenance. */
bool bongo_cat_plugin_worker_start(BongoCatPluginWorker *worker, const char *name,
    BongoCatPluginWork work, void *context, bool poll_idle);
bool bongo_cat_plugin_worker_submit(BongoCatPluginWorker *worker, const BongoCatPluginJob *job);
bool bongo_cat_plugin_worker_current(const BongoCatPluginWorker *worker);
bool bongo_cat_plugin_worker_busy(BongoCatPluginWorker *worker);
bool bongo_cat_plugin_worker_poll(BongoCatPluginWorker *worker, bool *failed, BongoCatError *error);
void bongo_cat_plugin_worker_stop(BongoCatPluginWorker *worker);
#endif
