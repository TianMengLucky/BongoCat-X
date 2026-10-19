#include "bongo_cat/lan.h"
#include "bongo_cat/path.h"
#include "bongo_cat/file.h"
#include "bongo_cat/log.h"
#include "bongo_cat/preferences.h"
#include "bongo_cat/i18n.h"
#include "bongo_cat/overlay.h"
#include "plugin_locale.h"
#include "lan_surface.h"
#include <string.h>
_Static_assert(sizeof(BongoCatLanModel) == 12353, "LAN model ABI mismatch");
_Static_assert(sizeof(BongoCatLanPose) == 80, "LAN pose ABI mismatch");
_Static_assert(sizeof(BongoCatLanSnapshot) == 12460, "LAN snapshot ABI mismatch");
struct BongoCatLan {
    SDL_SharedObject *library;
    const BongoCatLanPlugin *api;
    void *instance;
    struct {uint32_t id; BongoCatLanSurface *surface; uint64_t retry;} peers[BONGO_CAT_LAN_PEERS];
    BongoCatLanSnapshot snapshot, peer_snapshot;
    uint64_t frame;
    uint32_t generation, status;
    char session_model[BONGO_CAT_ID_CAP];
};
static const char *filename(void) {
#ifdef _WIN32
    return "bongo_lan.dll";
#elif defined(__APPLE__)
    return "libbongo_lan.dylib";
#else
    return "libbongo_lan.so";
#endif
}
static const BongoCatLanPlugin *query(SDL_SharedObject *library) {
    union { SDL_FunctionPointer function; BongoCatLanQuery query; } entry;
    entry.function = SDL_LoadFunction(library, BONGO_CAT_LAN_SYMBOL);
    const BongoCatLanPlugin *api = entry.function ? entry.query(2) : NULL;
    return api && api->abi_version == 2 && api->struct_size == sizeof(*api) &&
        api->create && api->destroy && api->disconnect && api->models && api->tick && api->peer && api->release &&
        api->friend_name && api->settings ? api : NULL;
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
void bongo_cat_lan_start(BongoCatApp *app) {
    if (!app || app->lan || app->secondary_pet) return;
    char path[BONGO_CAT_PATH_CAP], cache[BONGO_CAT_PATH_CAP];
    if (!resolve(app, path) || !bongo_cat_path_join(cache, sizeof(cache), app->cache_root, "lan")) return;
    SDL_SharedObject *library = SDL_LoadObject(path);
    const BongoCatLanPlugin *api = library ? query(library) : NULL;
    struct BongoCatLan *v = api ? SDL_calloc(1, sizeof(*v)) : NULL;
    if (v) { v->library = library; v->api = api; v->instance = api->create(cache); }
    if (!v || !v->instance) {
        SDL_free(v);
        if (library) SDL_UnloadObject(library);
        SDL_LogWarn(BONGO_CAT_LOG_LIFECYCLE, "[lan] Cannot load LAN plugin"); return;
    }
    v->snapshot.struct_size = sizeof(v->snapshot);
    app->lan = v;
    bongo_cat_plugin_locale_load(app->i18n, library, "lan");
    bongo_cat_preferences_plugin_fonts_changed(app->preferences);
    bongo_cat_preferences_invalidate(app->preferences);
}
void bongo_cat_lan_stop(BongoCatApp *app) {
    if (!app || !app->lan) return;
    struct BongoCatLan *v = app->lan;
    for (size_t i=0;i<BONGO_CAT_LAN_PEERS;++i) bongo_cat_lan_surface_destroy(&v->peers[i].surface);
    v->api->destroy(v->instance); /* Joins worker before cache and DLL retire. */
    bongo_cat_i18n_unregister_plugin(app->i18n, "lan");
    SDL_UnloadObject(v->library); SDL_free(v); app->lan = NULL;
}
void bongo_cat_lan_update(BongoCatApp *app) {
    struct BongoCatLan *v = app ? app->lan : NULL;
    uint64_t now = SDL_GetTicks();
    if (!v || now < v->frame) return;
    v->frame = now + 33;
    if (v->snapshot.status && strcmp(v->session_model, app->loaded_model))
        v->api->disconnect(v->instance);
    BongoCatLanPose pose = {0};
    pose.pointer_x = pose.pointer_y = 0.5f;
    (void)bongo_cat_overlay_pointer_snapshot(app->overlay, &pose.pointer_x, &pose.pointer_y, &pose.buttons);
    for (uint32_t i = 0; i < BONGO_CAT_LAN_PARAMETERS; ++i) {
        BongoCatParameterRange range;
        if (bongo_cat_model_runtime_parameter(app->model_runtime, bongo_cat_lan_parameters[i], &range)) {
            pose.mask |= 1u << i; pose.values[i] = range.value;
        }
    }
    v->api->tick(v->instance, &pose, &v->snapshot);
    if (v->snapshot.status != v->status) {
        v->status = v->snapshot.status;
        bongo_cat_preferences_invalidate(app->preferences);
    }
    if (v->generation != v->snapshot.generation) {
        v->generation = v->snapshot.generation;
        bongo_cat_preferences_invalidate(app->preferences);
    }
    for (size_t j=0;j<BONGO_CAT_LAN_PEERS;++j) {
        bool present=false;
        for (uint32_t i=0;i<v->snapshot.peer_count && i<BONGO_CAT_LAN_PEERS;++i) {
            v->peer_snapshot.struct_size=sizeof(v->peer_snapshot);
            if (v->api->peer(v->instance,i,&v->peer_snapshot) &&
                v->peer_snapshot.peer_id==v->peers[j].id) {present=true;break;}
        }
        if (!present) {
            bongo_cat_lan_surface_destroy(&v->peers[j].surface);
            v->peers[j].id=0;v->peers[j].retry=0;
        }
    }
    bool seen[BONGO_CAT_LAN_PEERS] = {false};
    for (uint32_t i=0;i<v->snapshot.peer_count && i<BONGO_CAT_LAN_PEERS;++i) {
        BongoCatLanSnapshot *peer = &v->peer_snapshot;
        peer->struct_size = sizeof(*peer);
        if (!v->api->peer(v->instance,i,peer)) continue;
        size_t slot = BONGO_CAT_LAN_PEERS;
        for (size_t j=0;j<BONGO_CAT_LAN_PEERS;++j)
            if (v->peers[j].id==peer->peer_id) {slot=j;break;}
        if (slot==BONGO_CAT_LAN_PEERS) {
            for (size_t j=0;j<BONGO_CAT_LAN_PEERS;++j)
                if (!v->peers[j].id) {slot=j;break;}
        }
        if (slot==BONGO_CAT_LAN_PEERS) continue;
        v->peers[slot].id=peer->peer_id;seen[slot]=true;
        const char *name=v->api->friend_name(v->instance,peer->peer_id);
        char fallback[128];
        if (!name || !name[0]) {
            SDL_snprintf(fallback,sizeof(fallback),"%s %u",bongo_cat_i18n_get(app->i18n,
                "pages.preference.lan.defaultFriend","Friend"),peer->peer_id);
            name=fallback;
        }
        if (now>=v->peers[slot].retry &&
            !bongo_cat_lan_surface_update(app,&v->peers[slot].surface,peer,name))
            v->peers[slot].retry=now+5000;
    }
    for (size_t i=0;i<BONGO_CAT_LAN_PEERS;++i) if (!seen[i]) {
        bongo_cat_lan_surface_destroy(&v->peers[i].surface);
        v->peers[i].id=0;v->peers[i].retry=0;
    }
    v->api->release(v->instance); /* Removed renderers retire before model caches. */
}
bool bongo_cat_lan_event(BongoCatApp *app, const SDL_Event *event) {
    if (!app || !app->lan) return false;
    for (size_t i=0;i<BONGO_CAT_LAN_PEERS;++i)
        if (bongo_cat_lan_surface_event(app->lan->peers[i].surface,event)) return true;
    return false;
}

uint32_t bongo_cat_lan_settings(BongoCatApp *app, BongoCatPluginUi *ui,
    const BongoCatPluginPageUiHost *host, const BongoCatPluginPageUiServices *services) {
    if (!app || !app->lan) return 0;
    struct BongoCatLan *v = app->lan;
    BongoCatLanModel *models = SDL_calloc(app->models.count, sizeof(*models));
    if (!app->models.count) (void)v->api->models(v->instance, NULL, 0, 0);
    if (models) {
        uint32_t selected = UINT32_MAX;
        for (size_t i = 0; i < app->models.count; ++i) {
            const BongoCatModelEntry *entry = &app->models.entries[i];
            SDL_strlcpy(models[i].directory, entry->directory, sizeof(models[i].directory));
            SDL_strlcpy(models[i].adapter, entry->adapter_directory, sizeof(models[i].adapter));
            SDL_strlcpy(models[i].setting, entry->setting_file, sizeof(models[i].setting));
            SDL_strlcpy(models[i].digest, entry->content_digest, sizeof(models[i].digest));
            if (!strcmp(entry->id, app->loaded_model)) selected = (uint32_t)i;
        }
        if (selected != UINT32_MAX)
            (void)v->api->models(v->instance, models, (uint32_t)app->models.count, selected);
        else (void)v->api->models(v->instance, NULL, 0, 0);
        SDL_free(models);
    }
    if (v->snapshot.status != 1 && v->snapshot.status != 2) SDL_strlcpy(v->session_model, app->loaded_model, sizeof(v->session_model));
    return v->api->settings(v->instance, ui, host, services);
}
bool bongo_cat_lan_import(BongoCatApp *app, const char *source,
    bool *matched, BongoCatError *error) {
    if (!matched) return false;
    *matched = false;
    if (!app || !source || app->secondary_pet) return false;
    size_t length = strlen(source);
    if (length < 4 || SDL_strcasecmp(source + length - 4, ".zip") == 0) return false;
    SDL_SharedObject *library = SDL_LoadObject(source);
    if (!library) { SDL_ClearError(); return false; }
    *matched = SDL_LoadFunction(library, BONGO_CAT_LAN_SYMBOL) != NULL;
    bool valid = *matched && query(library);
    SDL_UnloadObject(library);
    if (!*matched) { SDL_ClearError(); return false; }
    char folder[BONGO_CAT_PATH_CAP], target[BONGO_CAT_PATH_CAP], temporary[BONGO_CAT_PATH_CAP] = {0};
    bool ok = valid && !app->lan &&
        bongo_cat_path_join(folder, sizeof(folder), app->data_root, "plugins") &&
        bongo_cat_path_create_directory(folder) &&
        bongo_cat_path_join(target, sizeof(target), folder, filename());
    if (ok) {
        int n = SDL_snprintf(temporary, sizeof(temporary), "%s.new", target);
        ok = n > 0 && (size_t)n < sizeof(temporary) && SDL_CopyFile(source, temporary) &&
            bongo_cat_file_replace(temporary, target, true);
        if (!ok && temporary[0]) (void)bongo_cat_file_remove(temporary);
    }
    if (ok) { bongo_cat_lan_start(app); ok = app->lan != NULL; }
    if (!ok) bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
        "Cannot install LAN plugin; check ABI and restart before replacing an active plugin");
    return ok;
}

void bongo_cat_lan_release_graphics(BongoCatApp *app) {
    if (app && app->lan) {
        for (size_t i=0;i<BONGO_CAT_LAN_PEERS;++i) {
            bongo_cat_lan_surface_destroy(&app->lan->peers[i].surface);
            app->lan->peers[i].retry=0;
        }
        app->lan->frame = 0;
    }
}
