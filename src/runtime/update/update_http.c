#include "update_internal.h"
#include "bongo_cat/safe_http.h"

#include <stdio.h>
#include <string.h>

bool bongo_cat_update_cancelled(void *userdata) {
    BongoCatUpdateService *service = userdata;
    return SDL_GetAtomicInt(&service->http_cancelled) != 0;
}

BongoCatUpdateFetchResult bongo_cat_update_http_fetch(
    BongoCatUpdateService *service, char **response, size_t *length, char *error,
    size_t error_capacity) {
    *response = NULL;
    *length = 0;
    bongo_safe_http_client *client = bongo_safe_http_new();
    if (!client) {
        snprintf(error, error_capacity, "Cannot initialize the network client");
        return BONGO_CAT_UPDATE_FETCH_MEMORY;
    }
    unsigned char *bytes = NULL;
    int result = bongo_safe_http_get(client,
        "https://api.github.com/repos/TianMengLucky/BongoCat-X/releases/latest",
        service->accelerate_github, 1024u * 1024u,
        bongo_cat_update_cancelled, service, &bytes, length, error, error_capacity);
    bongo_safe_http_free(client);
    if (result == BONGO_SAFE_HTTP_OK) {
        if (memchr(bytes, 0, *length)) {
            bongo_safe_free_http_response(bytes, *length);
            snprintf(error, error_capacity, "Invalid GitHub response");
            return BONGO_CAT_UPDATE_FETCH_RESPONSE;
        }
        *response = (char *)bytes;
        return BONGO_CAT_UPDATE_FETCH_OK;
    }
    if (result == BONGO_SAFE_HTTP_CANCELLED) return BONGO_CAT_UPDATE_FETCH_CANCELLED;
    if (result == BONGO_SAFE_HTTP_NETWORK) return BONGO_CAT_UPDATE_FETCH_NETWORK;
    if (result == BONGO_SAFE_HTTP_MEMORY) return BONGO_CAT_UPDATE_FETCH_MEMORY;
    return BONGO_CAT_UPDATE_FETCH_RESPONSE;
}

void bongo_cat_update_http_cancel(BongoCatUpdateService *service) {
    if (service) SDL_SetAtomicInt(&service->http_cancelled, 1);
}
