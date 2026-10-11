#ifndef BONGO_CAT_SAFE_UPDATE_H
#define BONGO_CAT_SAFE_UPDATE_H

#include "bongo_cat/safe_http.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bongo_safe_update bongo_safe_update;

/* Downloads the package and its .sha256 sidecar, verifies it, and prepares a
   bounded archive in an isolated directory. Never changes the running app.
   The caller releases the opaque result, including its temporary files. */
int bongo_safe_update_prepare(const char *url, bool accelerate_github,
    const char *cache_root, const char *executable, bool installed,
    bongo_safe_cancelled cancelled, bongo_safe_http_progress progress,
    void *userdata, bongo_safe_update **update, char *message,
    size_t message_capacity);
/* Starts a hidden helper that waits for this process to exit before applying
   the package and relaunching. Only call after explicit user confirmation.
   False leaves the app running and the prepared update available for retry. */
bool bongo_safe_update_launch(bongo_safe_update *update, const char *nearby_root,
    char *message, size_t message_capacity);
void bongo_safe_update_free(bongo_safe_update *update);

#ifdef __cplusplus
}
#endif

#endif
