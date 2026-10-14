#include "update_internal.h"
#include "preferences_notice.h"
#include "bongo_cat/i18n.h"
#include "bongo_cat/log.h"
#include "bongo_cat/preferences.h"
#include "bongo_cat/safe_ffi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RELEASES_URL \
    "https://github.com/TianMengLucky/BongoCat-X/releases/latest"
#define STORE_URI "ms-windows-store://pdp/?ProductId=9P41MLSX72XW"
#define STORE_WEB_URL "https://apps.microsoft.com/detail/9P41MLSX72XW"

void bongo_cat_update_reap_worker(BongoCatUpdateService *service) {
    SDL_LockMutex(service->mutex);
    SDL_Thread *worker = service->status != BONGO_CAT_UPDATE_CHECKING &&
        service->status != BONGO_CAT_UPDATE_DOWNLOADING ? service->worker : NULL;
    if (worker) service->worker = NULL;
    SDL_UnlockMutex(service->mutex);
    if (worker) SDL_WaitThread(worker, NULL);
}

static int local_day(void) {
    SDL_Time timestamp = 0;
    SDL_DateTime local = {0};
    if (!SDL_GetCurrentTime(&timestamp) ||
        !SDL_TimeToDateTime(timestamp, &local, true)) return 0;
    return local.year * 10000 + local.month * 100 + local.day;
}

void bongo_cat_update_complete(BongoCatUpdateService *service,
    BongoCatUpdateStatus status, const BongoCatUpdateRelease *release,
    const char *error) {
    SDL_LockMutex(service->mutex);
    bool notify = !service->shutting_down;
    if (notify) {
        service->status = status;
        if (release) service->release = *release;
        else if (status != BONGO_CAT_UPDATE_ERROR)
            memset(&service->release, 0, sizeof(service->release));
        snprintf(service->error, sizeof(service->error), "%s",
            error ? error : "");
    }
    SDL_UnlockMutex(service->mutex);
    if (!notify) return;
    if (status == BONGO_CAT_UPDATE_ERROR)
        SDL_LogError(BONGO_CAT_LOG_UPDATE, "Update task failed: %s",
            error && error[0] ? error : "unknown error");
    else SDL_LogInfo(BONGO_CAT_LOG_UPDATE, "Update task: %s version=%s",
        status == BONGO_CAT_UPDATE_READY ? "downloaded" :
        status == BONGO_CAT_UPDATE_AVAILABLE ? "available" : "current",
        release ? release->version : BONGO_CAT_VERSION);
    SDL_Event event = {0};
    event.type = service->event_type;
    event.user.data1 = service;
    SDL_PushEvent(&event);
}

static int SDLCALL update_worker(void *userdata) {
    BongoCatUpdateService *service = userdata;
    char *response = NULL;
    size_t response_length = 0;
    char message[256] = {0};
    BongoCatUpdateFetchResult fetched = bongo_cat_update_http_fetch(service,
        &response, &response_length, message, sizeof(message));
    if (fetched != BONGO_CAT_UPDATE_FETCH_OK) {
        if (fetched != BONGO_CAT_UPDATE_FETCH_CANCELLED)
            bongo_cat_update_complete(service, BONGO_CAT_UPDATE_ERROR, NULL, message);
        return 0;
    }
    BongoCatUpdateRelease release;
    BongoCatError error = {0};
    bool parsed = bongo_cat_update_parse_release(response,
        bongo_cat_update_platform_asset(), &release, &error);
    bongo_safe_free_http_response((unsigned char *)response, response_length);
    if (!parsed) {
        bongo_cat_update_complete(service, BONGO_CAT_UPDATE_ERROR, NULL, error.message);
        return 0;
    }
    BongoCatUpdateStatus status = bongo_cat_update_compare_versions(
        release.version, BONGO_CAT_VERSION) > 0
        ? BONGO_CAT_UPDATE_AVAILABLE : BONGO_CAT_UPDATE_CURRENT;
    bongo_cat_update_complete(service, status, &release, NULL);
    return 0;
}

BongoCatUpdateService *bongo_cat_update_create(BongoCatApp *app) {
    if (!app) return NULL;
    BongoCatUpdateService *service = calloc(1, sizeof(*service));
    if (!service) return NULL;
    service->app = app;
    service->mutex = SDL_CreateMutex();
    service->event_type = SDL_RegisterEvents(1);
    if (!service->mutex ||
        service->event_type == (Uint32)-1) {
        if (service->mutex) SDL_DestroyMutex(service->mutex);
        free(service);
        return NULL;
    }
    if (!bongo_cat_update_platform_supported())
        service->status = BONGO_CAT_UPDATE_UNSUPPORTED;
    else if (bongo_cat_update_platform_store())
        service->status = BONGO_CAT_UPDATE_STORE;
    else {
        service->status = BONGO_CAT_UPDATE_IDLE;
        service->installed = bongo_cat_update_platform_installed();
        if (app->session.available_update_version[0] &&
            bongo_cat_update_compare_versions(
                app->session.available_update_version,
                BONGO_CAT_VERSION) > 0) {
            snprintf(service->release.version,
                sizeof(service->release.version), "%s",
                app->session.available_update_version);
            service->status = BONGO_CAT_UPDATE_AVAILABLE;
        } else {
            app->session.available_update_version[0] = '\0';
        }
    }
    return service;
}

bool bongo_cat_update_check(BongoCatUpdateService *service, bool manual) {
    if (!service) return false;
    bongo_cat_update_reap_worker(service);
    SDL_LockMutex(service->mutex);
    if (service->shutting_down || service->worker ||
        service->status == BONGO_CAT_UPDATE_STORE ||
        service->status == BONGO_CAT_UPDATE_UNSUPPORTED ||
        service->status == BONGO_CAT_UPDATE_READY) {
        SDL_UnlockMutex(service->mutex);
        return false;
    }
    service->status = BONGO_CAT_UPDATE_CHECKING;
    service->manual = manual;
    service->download_task = false;
    service->error[0] = '\0';
    SDL_SetAtomicInt(&service->http_cancelled, 0);
    service->accelerate_github =
        service->app->settings.app.github_download_acceleration;
    SDL_LogInfo(BONGO_CAT_LOG_UPDATE, "Update check started");
    service->worker = SDL_CreateThread(update_worker,
        BONGO_CAT_SLUG "-update-check", service);
    bool started = service->worker != NULL;
    if (!started) {
        service->status = BONGO_CAT_UPDATE_ERROR;
        SDL_LogError(BONGO_CAT_LOG_UPDATE, "Cannot start the update checker");
        snprintf(service->error, sizeof(service->error),
            "Cannot start the update checker");
    }
    SDL_UnlockMutex(service->mutex);
    return started;
}

bool bongo_cat_update_refresh_and_open(BongoCatUpdateService *service) {
    if (!service) return false;
    SDL_LockMutex(service->mutex);
    service->open_after_check = true;
    SDL_UnlockMutex(service->mutex);
    if (bongo_cat_update_check(service, true)) return true;
    SDL_LockMutex(service->mutex);
    service->open_after_check = false;
    SDL_UnlockMutex(service->mutex);
    return false;
}

void bongo_cat_update_start_automatic(BongoCatUpdateService *service) {
    if (!service || !service->app || service->app->smoke ||
        service->app->secondary_pet) return;
    if (service->automatic_started || !bongo_safe_auto_update_get(
            service->app->settings.extensions_json,
            sizeof(service->app->settings.extensions_json))) return;
    if (bongo_cat_update_check(service, false)) service->automatic_started = true;
}

void bongo_cat_update_snapshot(BongoCatUpdateService *service,
    BongoCatUpdateSnapshot *snapshot) {
    if (!snapshot) return;
    memset(snapshot, 0, sizeof(*snapshot));
    if (!service) {
        snapshot->status = BONGO_CAT_UPDATE_UNSUPPORTED;
        return;
    }
    SDL_LockMutex(service->mutex);
    snapshot->status = service->status;
    snapshot->release = service->release;
    snapshot->installed = service->installed;
    snapshot->downloaded_bytes = service->downloaded_bytes;
    snapshot->download_total_bytes = service->download_total_bytes;
    snprintf(snapshot->error, sizeof(snapshot->error), "%s", service->error);
    SDL_UnlockMutex(service->mutex);
}

static const char *tr(BongoCatUpdateService *service, const char *key,
    const char *fallback) {
    return bongo_cat_i18n_get(service->app->i18n, key, fallback);
}

bool bongo_cat_update_event(BongoCatUpdateService *service,
    const SDL_Event *event) {
    if (!service || !event || event->type != service->event_type ||
        event->user.data1 != service) return false;
    if (event->user.code == 1) {
        bongo_cat_preferences_invalidate(service->app->preferences);
        return true;
    }
    SDL_LockMutex(service->mutex);
    bool manual = service->manual;
    BongoCatUpdateStatus status = service->status;
    bool open_after_check = service->open_after_check;
    service->open_after_check = false;
    BongoCatUpdateRelease release = service->release;
    SDL_UnlockMutex(service->mutex);
    bongo_cat_update_reap_worker(service);
    bongo_cat_preferences_invalidate(service->app->preferences);
    if (status == BONGO_CAT_UPDATE_DOWNLOADING) return true;
    if (status == BONGO_CAT_UPDATE_READY) {
        bongo_cat_update_confirm(service);
        return true;
    }
    if ((status == BONGO_CAT_UPDATE_AVAILABLE ||
            status == BONGO_CAT_UPDATE_ERROR) && release.version[0])
        snprintf(service->app->session.available_update_version,
            sizeof(service->app->session.available_update_version), "%s",
            release.version);
    else if (status == BONGO_CAT_UPDATE_CURRENT)
        service->app->session.available_update_version[0] = '\0';
    if (!manual && (status == BONGO_CAT_UPDATE_CURRENT ||
            status == BONGO_CAT_UPDATE_AVAILABLE)) {
        int today = local_day();
        if (today) {
            BongoCatSessionState *session = &service->app->session;
            session->last_update_check_day = today;
            snprintf(session->last_update_check_version,
                sizeof(session->last_update_check_version), "%s",
                BONGO_CAT_VERSION);
        }
    }
    bongo_cat_preferences_invalidate(service->app->preferences);
    if (open_after_check && status == BONGO_CAT_UPDATE_AVAILABLE) {
        if (!bongo_cat_update_open(service)) {
            char detail[384];
            const char *reason = SDL_GetError();
            snprintf(detail, sizeof(detail), "%s: %s", tr(service,
                "native.support.downloadUpdateFailed",
                "Unable to download the update"),
                reason && reason[0] ? reason : "unknown error");
            bongo_cat_preferences_notice_show(service->app, detail, true);
        }
    } else if (manual) bongo_cat_update_show_completion(service);
    else if (status == BONGO_CAT_UPDATE_AVAILABLE && bongo_safe_auto_update_get(
            service->app->settings.extensions_json,
            sizeof(service->app->settings.extensions_json))) {
        char message[192];
        snprintf(message, sizeof(message), "%s v%s", tr(service,
            "native.support.updateAvailable", "New version available:"),
            release.version);
        bongo_cat_preferences_notice_show_update(service->app, message);
    }
    return true;
}

bool bongo_cat_update_open(BongoCatUpdateService *service) {
    BongoCatUpdateSnapshot snapshot;
    bongo_cat_update_snapshot(service, &snapshot);
    if (snapshot.status == BONGO_CAT_UPDATE_STORE)
        return SDL_OpenURL(STORE_URI) || SDL_OpenURL(STORE_WEB_URL);
    if (snapshot.status == BONGO_CAT_UPDATE_READY)
        return bongo_cat_update_confirm(service);
    if (snapshot.status == BONGO_CAT_UPDATE_AVAILABLE)
        return bongo_cat_update_begin_download(service);
    return SDL_OpenURL(RELEASES_URL);
}

void bongo_cat_update_destroy(BongoCatUpdateService *service) {
    if (!service) return;
    SDL_LockMutex(service->mutex);
    service->shutting_down = true;
    SDL_UnlockMutex(service->mutex);
    bongo_cat_update_http_cancel(service);
    if (service->worker) SDL_WaitThread(service->worker, NULL);
    SDL_Event event;
    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, service->event_type,
        service->event_type) > 0) {}
    bongo_safe_update_free(service->prepared_update);
    SDL_DestroyMutex(service->mutex);
    free(service);
}
