#include "preferences_state.h"
#include "preferences_live2d_setup.h"
#include "preferences_notice.h"
#include "bongo_cat/path.h"
#include "bongo_cat/file.h"
#include "bongo_cat/safe_http.h"
#include "bongo_cat/log.h"
#include "bongo_cat/platform.h"
#include <stdio.h>
#include <string.h>
static const char *label(BongoCatPreferences *value, const char *key, const char *fallback) {
    char full[128]; snprintf(full, sizeof(full), "pages.preference.live2dSetup.%s", key);
    return bongo_cat_i18n_get(value->app->i18n, full, fallback);
}
static bool cancelled(void *context) {
    return atomic_load_explicit(&((BongoCatLive2DSetup *)context)->cancelled, memory_order_acquire);
}
#ifdef _WIN32
static void progress(void *context, uint64_t received, uint64_t total) {
    BongoCatLive2DSetup *job = context;
    atomic_store(&job->received, received); atomic_store(&job->total, total);
}
#endif
static bool licenses(BongoCatLive2DSetup *job) {
    bongo_safe_http_client *client = bongo_safe_http_new();
    if (!client) { SDL_strlcpy(job->message, "Cannot initialize HTTPS client", sizeof(job->message)); return false; }
    char *text = SDL_malloc(98304);
    bool ok = text != NULL;
    const char *names[] = {"proprietary", "open"};
    for (unsigned i = 0; ok && i < 2; ++i) {
        char url[256]; unsigned char *bytes = NULL; size_t length = 0;
        snprintf(url, sizeof(url), "https://www.live2d.com/eula/live2d-%s-software-license-agreement_%s.html", names[i], job->language);
        ok = bongo_safe_http_get(client, url, false, 1024u * 1024u, cancelled, job,
            &bytes, &length, job->message, sizeof(job->message)) == BONGO_SAFE_HTTP_OK &&
            bongo_safe_live2d_license_text(bytes, length, text, 98304);
        if (ok) {
            size_t used = strlen(job->terms), added = strlen(text);
            ok = added + used + 3 < sizeof(job->terms);
            if (ok) { SDL_strlcat(job->terms, text, sizeof(job->terms)); SDL_strlcat(job->terms, "\n\n", sizeof(job->terms)); }
        }
        bongo_safe_free_http_response(bytes, length);
    }
    SDL_free(text); bongo_safe_http_free(client);
    if (ok) job->licenses_ready = true;
    else if (!job->message[0]) SDL_strlcpy(job->message, "Cannot read both official license agreements", sizeof(job->message));
    return ok;
}
static int SDLCALL run(void *context) {
    BongoCatLive2DSetup *job = context;
    bool downloading = atomic_load(&job->phase) == BONGO_CAT_SDK_DOWNLOADING;
    bool ok;
    if (downloading) {
#ifdef _WIN32
        ok = bongo_safe_live2d_download(job->target, sizeof(void *) == 8, cancelled,
            progress, job, job->message, sizeof(job->message)) == BONGO_SAFE_HTTP_OK;
#else
        SDL_strlcpy(job->message, "Automatic Core installation is unavailable on this platform", sizeof(job->message)); ok = false;
#endif
    } else ok = licenses(job);
    atomic_store_explicit(&job->phase, ok ? downloading ? BONGO_CAT_SDK_COMPLETE :
        BONGO_CAT_SDK_LICENSE_READY : BONGO_CAT_SDK_FAILED, memory_order_release);
    if (job->event_type) { SDL_Event event = {0}; event.type = job->event_type; (void)SDL_PushEvent(&event); }
    return 0;
}
bool bongo_cat_preferences_live2d_setup_active(const BongoCatPreferences *value) {
    return value && value->live2d_setup && value->live2d_setup->open;
}
const char *bongo_cat_preferences_live2d_setup_terms(const BongoCatPreferences *value) {
    if (!value || !value->live2d_setup) return NULL;
    BongoCatLive2DSetup *job = value->live2d_setup;
    if (atomic_load_explicit(&job->phase, memory_order_acquire) == BONGO_CAT_SDK_LICENSE_LOADING) return NULL;
    return job->licenses_ready ? job->terms : NULL;
}
void bongo_cat_preferences_live2d_setup_stop(BongoCatPreferences *value) {
    if (!value || !value->live2d_setup) return;
    BongoCatLive2DSetup *job = value->live2d_setup;
    atomic_store(&job->cancelled, true);
    if (job->thread) SDL_WaitThread(job->thread, NULL);
    if (job->target[0]) (void)bongo_cat_file_remove(job->target);
    SDL_free(job); value->live2d_setup = NULL;
}
void bongo_cat_preferences_live2d_setup_request(BongoCatPreferences *value) {
    if (!value || bongo_cat_preferences_live2d_setup_active(value)) return;
    bongo_cat_preferences_live2d_setup_stop(value);
    BongoCatLive2DSetup *job = SDL_calloc(1, sizeof(*job));
    if (!job) return;
    atomic_init(&job->phase, BONGO_CAT_SDK_LICENSE_LOADING); atomic_init(&job->cancelled, false);
    atomic_init(&job->received, 0); atomic_init(&job->total, 0);
    job->open = true; job->opened_ns = SDL_GetTicksNS(); job->event_type = (unsigned)value->live2d_setup_event_type;
    bool chinese = value->app->settings.app.language == BONGO_CAT_LANG_ZH_CN || value->app->settings.app.language == BONGO_CAT_LANG_ZH_HANT;
    SDL_strlcpy(job->language, chinese ? "cn" : "en", sizeof(job->language));
    char directory[BONGO_CAT_PATH_CAP];
    bool valid = bongo_cat_path_join(directory, sizeof(directory), value->app->data_root, "live2d/downloads") &&
        bongo_cat_path_join(job->target, sizeof(job->target), directory, "Live2DCubismCore.dll");
    value->live2d_setup = job;
    if (valid) job->thread = SDL_CreateThread(run, "live2d-license", job);
    if (!job->thread) {
        SDL_strlcpy(job->message, "Cannot start Live2D setup", sizeof(job->message)); atomic_store(&job->phase, BONGO_CAT_SDK_FAILED);
    }
    value->render_dirty = true;
}
void bongo_cat_preferences_live2d_setup_cancel(BongoCatPreferences *value) {
    if (!value || !value->live2d_setup) return;
    value->live2d_setup->open = false;
    atomic_store(&value->live2d_setup->cancelled, true);
    value->render_dirty = true;
}
void bongo_cat_preferences_live2d_setup_accept(BongoCatPreferences *value) {
    if (!bongo_cat_preferences_live2d_setup_active(value)) return;
    BongoCatLive2DSetup *job = value->live2d_setup;
    int phase = atomic_load_explicit(&job->phase, memory_order_acquire);
    if ((phase != BONGO_CAT_SDK_LICENSE_READY && phase != BONGO_CAT_SDK_FAILED) || !job->licenses_ready) return;
    if (job->thread) SDL_WaitThread(job->thread, NULL);
    job->thread = NULL; job->message[0] = 0;
    atomic_store(&job->cancelled, false); atomic_store(&job->phase, BONGO_CAT_SDK_DOWNLOADING);
    job->thread = SDL_CreateThread(run, "live2d-download", job);
    if (!job->thread) atomic_store(&job->phase, BONGO_CAT_SDK_FAILED);
    value->render_dirty = true;
}
void bongo_cat_preferences_live2d_setup_poll(BongoCatPreferences *value) {
    if (!value || !value->live2d_setup) return;
    BongoCatLive2DSetup *job = value->live2d_setup;
    int phase = atomic_load_explicit(&job->phase, memory_order_acquire);
    if (phase == BONGO_CAT_SDK_LICENSE_LOADING || phase == BONGO_CAT_SDK_DOWNLOADING) return;
    if (!job->open) { bongo_cat_preferences_live2d_setup_stop(value); return; }
    if (phase != (int)job->observed_phase) {
        job->observed_phase = (unsigned)phase; value->render_dirty = true;
        if (phase == BONGO_CAT_SDK_LICENSE_READY) value->font_reload_pending = true;
    }
    if (phase != BONGO_CAT_SDK_COMPLETE) return;
    if (job->thread) { SDL_WaitThread(job->thread, NULL); job->thread = NULL; }
    BongoCatError error = {0};
    bool ok = bongo_cat_platform_live2d_core_available() ? bongo_cat_app_activate_live2d_core(value->app, &error) :
        bongo_cat_app_import_live2d_core(value->app, job->target, &error);
    if (ok) {
        bongo_cat_preferences_notice_clear(value->app);
        bongo_cat_preferences_notice_show(value->app, label(value, "installed", "Live2D Core installed; rendering is enabled"), false);
        (void)bongo_cat_file_remove(job->target);
        bongo_cat_preferences_live2d_setup_stop(value);
    } else {
        SDL_strlcpy(job->message, error.message, sizeof(job->message));
        atomic_store(&job->phase, BONGO_CAT_SDK_FAILED);
        SDL_LogWarn(BONGO_CAT_LOG_LIFECYCLE, "Live2D setup: %s", error.message);
    }
    value->render_dirty = true;
}
