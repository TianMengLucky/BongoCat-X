#include "update_internal.h"
#include "bongo_cat/i18n.h"
#include "bongo_cat/log.h"
#include "bongo_cat/preferences.h"
#include "preferences_notice.h"

#include <stdio.h>
#include <string.h>

static const char *tr(BongoCatUpdateService *service, const char *key,
    const char *fallback) {
    return bongo_cat_i18n_get(service->app->i18n, key, fallback);
}

static void progress(void *userdata, uint64_t received, uint64_t total) {
    BongoCatUpdateService *service = userdata;
    Uint64 now = SDL_GetTicksNS();
    SDL_LockMutex(service->mutex);
    service->downloaded_bytes = received;
    service->download_total_bytes = total;
    bool notify = !service->shutting_down &&
        (now - service->progress_event_ns >= 100 * SDL_NS_PER_MS);
    if (notify) service->progress_event_ns = now;
    SDL_UnlockMutex(service->mutex);
    if (notify) {
        SDL_Event event = {0};
        event.type = service->event_type;
        event.user.code = 1;
        event.user.data1 = service;
        SDL_PushEvent(&event);
    }
}

static int SDLCALL download_worker(void *userdata) {
    BongoCatUpdateService *service = userdata;
    BongoCatUpdateSnapshot snapshot;
    bongo_cat_update_snapshot(service, &snapshot);
    const char *url = snapshot.installed ? snapshot.release.installer_url :
        snapshot.release.portable_url;
    char error[256] = {0};
    bongo_safe_update *prepared = NULL;
    int result = bongo_safe_update_prepare(url, service->accelerate_github,
        service->app->cache_root, service->app->executable_path,
        snapshot.installed, bongo_cat_update_cancelled, progress, service,
        &prepared, error, sizeof(error));
    SDL_LockMutex(service->mutex);
    service->prepared_update = prepared;
    SDL_UnlockMutex(service->mutex);
    if (result != BONGO_SAFE_HTTP_CANCELLED)
        bongo_cat_update_complete(service, result == BONGO_SAFE_HTTP_OK ?
            BONGO_CAT_UPDATE_READY : BONGO_CAT_UPDATE_ERROR,
            &snapshot.release, error);
    return 0;
}

bool bongo_cat_update_begin_download(BongoCatUpdateService *service) {
    bongo_cat_update_reap_worker(service);
    SDL_LockMutex(service->mutex);
    const char *url = service->installed ? service->release.installer_url :
        service->release.portable_url;
    if (service->shutting_down || service->worker ||
        service->status != BONGO_CAT_UPDATE_AVAILABLE || !url[0]) {
        SDL_UnlockMutex(service->mutex);
        SDL_SetError("No matching update package is available");
        return false;
    }
    service->status = BONGO_CAT_UPDATE_DOWNLOADING;
    service->manual = true;
    service->download_task = true;
    service->error[0] = '\0';
    service->downloaded_bytes = service->download_total_bytes = 0;
    service->progress_event_ns = 0;
    service->accelerate_github =
        service->app->settings.app.github_download_acceleration;
    SDL_SetAtomicInt(&service->http_cancelled, 0);
    service->worker = SDL_CreateThread(download_worker,
        BONGO_CAT_SLUG "-update-download", service);
    bool started = service->worker != NULL;
    if (!started) {
        service->status = BONGO_CAT_UPDATE_ERROR;
        snprintf(service->error, sizeof(service->error),
            "Cannot start the update downloader");
        SDL_SetError("Cannot start the update downloader");
    }
    SDL_UnlockMutex(service->mutex);
    bongo_cat_preferences_invalidate(service->app->preferences);
    SDL_LogInfo(BONGO_CAT_LOG_UPDATE, "Update download %s", started ?
        "started" : "failed to start");
    return started;
}

bool bongo_cat_update_confirm(BongoCatUpdateService *service) {
    if (!service) return false;
    bongo_cat_update_reap_worker(service);
    SDL_LockMutex(service->mutex);
    bongo_safe_update *prepared = service->prepared_update;
    bool ready = service->status == BONGO_CAT_UPDATE_READY && prepared;
    SDL_UnlockMutex(service->mutex);
    if (!ready) return false;
    char prompt[512];
    snprintf(prompt, sizeof(prompt), tr(service,
        "native.support.updateRestartPrompt",
        "Version %s has been downloaded and verified. Restart now to update BongoCat?"),
        service->release.version);
    SDL_MessageBoxButtonData buttons[] = {
        {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT |
            SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, tr(service,
            "native.support.updateLater", "Later")},
        {0, 1, tr(service,
            "native.support.restartUpdate", "Restart and Update")}
    };
    SDL_MessageBoxData dialog = {0};
    dialog.flags = SDL_MESSAGEBOX_INFORMATION;
    dialog.window = service->app->window;
    dialog.title = tr(service, "native.support.updateDownloaded",
        "Update downloaded");
    dialog.message = prompt;
    dialog.numbuttons = 2;
    dialog.buttons = buttons;
    int selected = 0;
    if (!SDL_ShowMessageBox(&dialog, &selected)) {
        bongo_cat_preferences_notice_show(service->app, tr(service,
            "native.support.updateDialogFailed", "Unable to show the update confirmation"), true);
        return false;
    }
    if (selected != 1) return true;
    char error[256] = {0};
    if (!bongo_safe_update_launch(prepared, service->app->nearby_root,
        error, sizeof(error))) {
        char message[512];
        snprintf(message, sizeof(message), "%s: %s", tr(service,
            "native.support.updateApplyFailed", "Unable to start the update"), error);
        bongo_cat_preferences_notice_show(service->app, message, true);
        SDL_LogError(BONGO_CAT_LOG_UPDATE, "%s", error);
        return false;
    }
    /* Normal shutdown flushes configuration and stops all secondary pets before
       the helper replaces files. The prepared directory now belongs to it. */
    service->app->running = false;
    return true;
}
