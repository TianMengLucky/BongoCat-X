#ifndef BONGO_CAT_SAFE_WORKER_H
#define BONGO_CAT_SAFE_WORKER_H
#include <stdbool.h>
#include <stddef.h>
typedef bool (*BongoSafeWork)(void *context, const void *job, void *error);
#ifdef __cplusplus
extern "C" {
#endif
void *bongo_safe_worker_new(const char *name, BongoSafeWork work, void *context,
    bool poll_idle, size_t job_size, size_t error_size);
bool bongo_safe_worker_submit(void *worker, const void *job);
bool bongo_safe_worker_busy(const void *worker);
bool bongo_safe_worker_current(const void *worker);
bool bongo_safe_worker_poll(void *worker, bool *failed, void *error);
/* Joins before returning; context, plugin code and callbacks must outlive it. */
void bongo_safe_worker_free(void *worker);
#ifdef __cplusplus
}
#endif
#endif
