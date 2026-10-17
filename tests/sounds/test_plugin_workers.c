#include "test.h"
#include "plugin_worker.h"
#include <stdatomic.h>
int bongo_cat_test_failures;
typedef struct State { SDL_Mutex *mutex; SDL_Condition *wake; bool blocked, release; atomic_uint started, done, idle; } State;
static bool work(void *context, const BongoCatPluginJob *job, BongoCatError *error) {
    (void)error;
    State *state = context;
    if (job->kind == 0) atomic_fetch_add(&state->idle, 1);
    if (job->kind != 1) return true;
    atomic_fetch_add(&state->started, 1);
    SDL_LockMutex(state->mutex);
    while (state->blocked && !state->release) SDL_WaitCondition(state->wake, state->mutex);
    SDL_UnlockMutex(state->mutex);
    atomic_fetch_add(&state->done, 1); return true;
}
static void wait_count(atomic_uint *value) {
    uint64_t deadline = SDL_GetTicks() + 2000;
    while (!atomic_load(value) && SDL_GetTicks() < deadline) SDL_Delay(1);
    CHECK(atomic_load(value) != 0);
}
typedef struct OverflowState {
    SDL_Mutex *mutex;
    SDL_Condition *wake;
    bool release_job, release_reset;
    atomic_uint started, reset, done, closed;
} OverflowState;
static bool overflow_work(void *context, const BongoCatPluginJob *job, BongoCatError *error) {
    (void)error;
    OverflowState *state = context;
    if (job->kind == 1 || job->kind == 255) {
        atomic_fetch_add(job->kind == 1 ? &state->started : &state->reset, 1);
        SDL_LockMutex(state->mutex);
        while (!(job->kind == 1 ? state->release_job : state->release_reset))
            SDL_WaitCondition(state->wake, state->mutex);
        SDL_UnlockMutex(state->mutex);
    }
    if (job->kind == 4) atomic_fetch_add(&state->done, 1);
    if (job->kind == 254) atomic_fetch_add(&state->closed, 1);
    return true;
}
static void test_overflow(void) {
    OverflowState state = {0};
    atomic_init(&state.started, 0); atomic_init(&state.reset, 0);
    atomic_init(&state.done, 0); atomic_init(&state.closed, 0);
    state.mutex = SDL_CreateMutex(); state.wake = SDL_CreateCondition();
    CHECK(state.mutex && state.wake);
    if (!state.mutex || !state.wake) {
        SDL_DestroyCondition(state.wake); SDL_DestroyMutex(state.mutex); return;
    }
    BongoCatPluginWorker worker;
    bool ready = bongo_cat_plugin_worker_start(&worker, "test-overflow-worker", overflow_work, &state, false);
    CHECK(ready);
    if (ready) {
        BongoCatPluginJob job = {0}; job.kind = 1;
        CHECK(bongo_cat_plugin_worker_submit(&worker, &job)); wait_count(&state.started);
        job.kind = 4;
        for (unsigned i = 0; i < BONGO_CAT_PLUGIN_JOBS; ++i)
            CHECK(bongo_cat_plugin_worker_submit(&worker, &job));
        CHECK(!bongo_cat_plugin_worker_submit(&worker, &job));
        CHECK(bongo_cat_plugin_worker_busy(&worker));
        SDL_LockMutex(state.mutex); state.release_job = true;
        SDL_BroadcastCondition(state.wake); SDL_UnlockMutex(state.mutex);
        wait_count(&state.reset);
        CHECK(atomic_load(&state.done) == BONGO_CAT_PLUGIN_JOBS);
        CHECK(bongo_cat_plugin_worker_busy(&worker)); /* Reset is still executing. */
        SDL_LockMutex(state.mutex); state.release_reset = true;
        SDL_BroadcastCondition(state.wake); SDL_UnlockMutex(state.mutex);
        bongo_cat_plugin_worker_stop(&worker);
        CHECK(atomic_load(&state.reset) == 1);
        CHECK(atomic_load(&state.closed) == 1);
        CHECK(!bongo_cat_plugin_worker_busy(&worker));
    }
    SDL_DestroyCondition(state.wake); SDL_DestroyMutex(state.mutex);
}
int main(void) {
    CHECK(SDL_Init(0));
    test_overflow();
    State first = {0}, second = {0}; first.blocked = true;
    first.mutex = SDL_CreateMutex(); first.wake = SDL_CreateCondition();
    second.mutex = SDL_CreateMutex(); second.wake = SDL_CreateCondition();
    atomic_init(&first.started, 0); atomic_init(&first.done, 0); atomic_init(&first.idle, 0);
    atomic_init(&second.started, 0); atomic_init(&second.done, 0); atomic_init(&second.idle, 0);
    BongoCatPluginWorker a, b;
    bool ready_a = bongo_cat_plugin_worker_start(&a, "test-items-worker", work, &first, false);
    bool ready_b = bongo_cat_plugin_worker_start(&b, "test-sounds-worker", work, &second, true);
    CHECK(ready_a && ready_b);
    if (ready_a && ready_b) {
        wait_count(&second.idle);
        CHECK(atomic_load(&first.idle) == 0);
        CHECK(!bongo_cat_plugin_worker_current(&a) && !bongo_cat_plugin_worker_current(&b));
        BongoCatPluginJob job = {0}; job.kind = 1;
        CHECK(bongo_cat_plugin_worker_submit(&a, &job)); wait_count(&first.started);
        CHECK(bongo_cat_plugin_worker_submit(&b, &job)); wait_count(&second.done);
        CHECK(atomic_load(&first.done) == 0); /* Slow item work cannot stall sounds. */
    }
    SDL_LockMutex(first.mutex); first.release = true; SDL_BroadcastCondition(first.wake); SDL_UnlockMutex(first.mutex);
    bongo_cat_plugin_worker_stop(&a); bongo_cat_plugin_worker_stop(&b);
    SDL_DestroyCondition(first.wake); SDL_DestroyCondition(second.wake);
    SDL_DestroyMutex(first.mutex); SDL_DestroyMutex(second.mutex); SDL_Quit();
    return bongo_cat_test_failures ? 1 : 0;
}
