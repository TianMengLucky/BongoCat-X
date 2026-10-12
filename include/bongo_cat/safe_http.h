#ifndef BONGO_CAT_SAFE_HTTP_H
#define BONGO_CAT_SAFE_HTTP_H

#include "bongo_cat/safe_ffi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bongo_safe_http_client bongo_safe_http_client;
typedef void (*bongo_safe_http_progress)(void *userdata, uint64_t received,
    uint64_t total);

enum bongo_safe_http_result {
    BONGO_SAFE_HTTP_OK, BONGO_SAFE_HTTP_CANCELLED, BONGO_SAFE_HTTP_NETWORK,
    BONGO_SAFE_HTTP_RESPONSE, BONGO_SAFE_HTTP_MEMORY
};

/* A client belongs to one worker; requests reuse its connection pool. Only
   public HTTPS resources are accepted. Cancellation is polled during DNS,
   connection, headers and body reads. Returned bytes include a NUL terminator
   beyond length; binary bodies remain valid. No response is published on error. */
bongo_safe_http_client *bongo_safe_http_new(void);
void bongo_safe_http_free(bongo_safe_http_client *client);
int bongo_safe_http_get(bongo_safe_http_client *client, const char *url,
    bool accelerate_github, size_t limit, bongo_safe_cancelled cancelled,
    void *userdata, unsigned char **response, size_t *length,
    char *message, size_t message_capacity);
void bongo_safe_free_http_response(unsigned char *response, size_t length);

#ifdef __cplusplus
}
#endif

#endif
