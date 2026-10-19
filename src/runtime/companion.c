#include "bongo_cat/companion.h"
#include "bongo_cat/path.h"
#include "bongo_cat/file.h"
#include "bongo_cat/log.h"
#include "bongo_cat/preferences.h"
#include "bongo_cat/safe_http.h"
#include "bongo_cat/i18n.h"
#include "plugin_locale.h"
#include "plugin_overlay.h"
#include "following_overlay.h"
#include <string.h>

typedef struct BongoCatCompanion {
    SDL_SharedObject *library;
    const BongoCatCompanionPlugin *api;
    const BongoCatPluginOverlay *overlay;
    void *instance;
    BongoCatFollowingOverlay *surface;
    uint64_t retry;
    BongoCatCompanionSnapshot snapshot;
    BongoCatCompanionTts tts;
    uint32_t spoken;
    bool speaking;
} BongoCatCompanion;
static const char *filename(void) {
#ifdef _WIN32
    return "bongo_companion.dll";
#elif defined(__APPLE__)
    return "libbongo_companion.dylib";
#else
    return "libbongo_companion.so";
#endif
}
static const BongoCatCompanionPlugin *query(SDL_SharedObject *library) {
    union { SDL_FunctionPointer function; BongoCatCompanionQuery query; } entry;
    entry.function = SDL_LoadFunction(library, BONGO_CAT_COMPANION_SYMBOL);
    const BongoCatCompanionPlugin *api = entry.function ? entry.query(1) : NULL;
    return api && api->abi_version == 1 && api->struct_size == sizeof(*api) &&
        api->create && api->destroy && api->event && api->snapshot && api->settings ? api : NULL;
}
static uint32_t replace(const char *from, const char *to) {
    return bongo_cat_file_replace(from, to, true);
}
static bool resolve(BongoCatApp *app, char *out) {
    char folder[BONGO_CAT_PATH_CAP];
    const char *override = SDL_getenv("BONGO_CAT_PLUGIN_DIRECTORY");
    if (override && override[0]) return bongo_cat_path_join(out, BONGO_CAT_PATH_CAP,
        override, filename()) && bongo_cat_path_is_file(out);
    if (bongo_cat_path_join(folder, sizeof(folder), app->data_root, "plugins") &&
        bongo_cat_path_join(out, BONGO_CAT_PATH_CAP, folder, filename()) &&
        bongo_cat_path_is_file(out)) return true;
    const char *base = SDL_GetBasePath(); /* SDL3 owns this pointer. */
    if (!base) return false;
    if (bongo_cat_path_join(folder, sizeof(folder), base, "plugins") &&
        bongo_cat_path_join(out, BONGO_CAT_PATH_CAP, folder, filename()) &&
        bongo_cat_path_is_file(out)) return true;
#ifdef __APPLE__
    if (bongo_cat_path_join(folder, sizeof(folder), base, "../PlugIns") &&
        bongo_cat_path_join(out, BONGO_CAT_PATH_CAP, folder, filename()) &&
        bongo_cat_path_is_file(out)) return true;
#endif
    return false;
}
void bongo_cat_companion_start(BongoCatApp *app) {
    if (!app || app->companion || app->secondary_pet) return;
    char path[BONGO_CAT_PATH_CAP];
    if (!resolve(app, path)) return;
    SDL_SharedObject *library = SDL_LoadObject(path);
    const BongoCatCompanionPlugin *api = library ? query(library) : NULL;
    const BongoCatPluginOverlay *overlay = api ? bongo_cat_plugin_overlay_query(library,
        "bongo_cat_companion_overlay_query_v1") : NULL;
    const BongoCatCompanionHost host = {1, sizeof(host), bongo_safe_http_post_json,
        bongo_safe_free_http_response, replace};
    BongoCatCompanion *v = overlay ? SDL_calloc(1, sizeof(*v)) : NULL;
    if (v) {
        v->library = library; v->api = api; v->overlay = overlay;
        v->instance = api->create(app->data_root, &host);
        v->snapshot.struct_size = sizeof(v->snapshot);
    }
    if (!v || !v->instance) {
        SDL_free(v);
        if (library) SDL_UnloadObject(library);
        SDL_LogWarn(BONGO_CAT_LOG_LIFECYCLE, "[companion] Cannot load experimental plugin or settings");
        return;
    }
    app->companion = v;
    bongo_cat_plugin_locale_load(app->i18n, library, "companion");
    bongo_cat_preferences_plugin_fonts_changed(app->preferences);
    bongo_cat_preferences_invalidate(app->preferences);
}
void bongo_cat_companion_stop(BongoCatApp *app) {
    if (!app || !app->companion) return;
    BongoCatCompanion *v = app->companion;
    bongo_cat_following_overlay_destroy(&v->surface);
    if (v->tts.cancel) v->tts.cancel(v->tts.context);
    v->api->destroy(v->instance); /* Cancel/join before callbacks and DLL retire. */
    bongo_cat_i18n_unregister_plugin(app->i18n, "companion");
    SDL_UnloadObject(v->library);
    SDL_free(v); app->companion = NULL;
}
bool bongo_cat_companion_active(BongoCatApp *app) {
    return app && app->companion;
}
void bongo_cat_companion_event(BongoCatApp *app, uint32_t event) {
    if (app && app->companion)
        app->companion->api->event(app->companion->instance, event, SDL_GetTicks());
}
void bongo_cat_companion_update(BongoCatApp *app) {
    BongoCatCompanion *v = app ? app->companion : NULL;
    if (!v) return;
    uint32_t busy = v->snapshot.busy, status = v->snapshot.status;
    uint32_t sequence = v->snapshot.sequence;
    bongo_cat_companion_event(app, BONGO_COMPANION_TICK);
    if (!v->api->snapshot(v->instance, &v->snapshot)) return;
    if (busy != v->snapshot.busy || status != v->snapshot.status || sequence != v->snapshot.sequence)
        bongo_cat_preferences_invalidate(app->preferences);
    v->snapshot.text[sizeof(v->snapshot.text) - 1] = 0;
    if (v->snapshot.sequence != v->spoken) {
        v->spoken = v->snapshot.sequence;
        if (v->snapshot.visible && v->tts.speak) {
            if (v->speaking) v->tts.cancel(v->tts.context);
            v->tts.speak(v->tts.context, v->snapshot.text); v->speaking = true;
        }
    }
    if (!v->snapshot.visible && v->speaking) {
        v->tts.cancel(v->tts.context); v->speaking = false;
    }
    bongo_cat_following_overlay_update(app, &v->surface, v->overlay, v->instance,
        v->snapshot.visible != 0, true, &v->retry);
}
bool bongo_cat_companion_overlay_event(BongoCatApp *app, const SDL_Event *event) {
    return app && app->companion &&
        bongo_cat_following_overlay_event(app->companion->surface, event);
}
uint32_t bongo_cat_companion_settings(BongoCatApp *app, BongoCatPluginUi *ui,
    const BongoCatPluginPageUiHost *host, const BongoCatPluginPageUiServices *services) {
    return app && app->companion ? app->companion->api->settings(
        app->companion->instance, ui, host, services) : 0;
}
bool bongo_cat_companion_set_tts(BongoCatApp *app, const BongoCatCompanionTts *tts) {
    if (!app || !app->companion || (tts && (tts->abi_version != 1 ||
        tts->struct_size != sizeof(*tts) || !tts->speak || !tts->cancel))) return false;
    BongoCatCompanion *v = app->companion;
    if (v->tts.cancel) v->tts.cancel(v->tts.context);
    v->tts = tts ? *tts : (BongoCatCompanionTts){0};
    v->spoken = v->snapshot.sequence; v->speaking = false;
    return true;
}
bool bongo_cat_companion_import(BongoCatApp *app, const char *source,
    bool *matched, BongoCatError *error) {
    if (!matched) return false;
    *matched = false;
    if (!app || !source || app->secondary_pet || SDL_strcasecmp(source +
        (strlen(source) > 4 ? strlen(source) - 4 : 0), ".zip") == 0) return false;
    SDL_SharedObject *library = SDL_LoadObject(source);
    if (!library) { SDL_ClearError(); return false; }
    *matched = SDL_LoadFunction(library, BONGO_CAT_COMPANION_SYMBOL) != NULL;
    bool valid = *matched && query(library) && bongo_cat_plugin_overlay_query(library,
        "bongo_cat_companion_overlay_query_v1");
    SDL_UnloadObject(library);
    if (!*matched) { SDL_ClearError(); return false; }
    char folder[BONGO_CAT_PATH_CAP], target[BONGO_CAT_PATH_CAP];
    bool ok = valid && !app->companion &&
        bongo_cat_path_join(folder, sizeof(folder), app->data_root, "plugins") &&
        bongo_cat_path_create_directory(folder) &&
        bongo_cat_path_join(target, sizeof(target), folder, filename());
    if (ok) {
        /* Reuse durable replacement. Source is copied; the user's library stays intact. */
        char temporary[BONGO_CAT_PATH_CAP] = {0};
        int length = SDL_snprintf(temporary, sizeof(temporary), "%s.new", target);
        ok = length > 0 && (size_t)length < sizeof(temporary) &&
            bongo_cat_path_copy_file(source, temporary) && bongo_cat_file_replace(temporary, target, true);
        if (!ok) (void)bongo_cat_file_remove(temporary);
    }
    if (ok) { bongo_cat_companion_start(app); ok = app->companion != NULL; }
    if (!ok) bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
        "Cannot install experimental companion plugin; check ABI and restart before replacing an active plugin");
    return ok;
}
