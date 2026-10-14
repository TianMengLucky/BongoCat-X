#include "items_internal.h"
#include "bongo_cat/path.h"
#include "bongo_cat/file.h"
#include "bongo_cat/log.h"
#include "bongo_cat/preferences.h"
#include "bongo_cat/overlay.h"
#include "runtime.h"
#include <stdio.h>
#include <string.h>
const char *bongo_cat_items_filename(void) {
#ifdef _WIN32
    return "bongo_items.dll";
#elif defined(__APPLE__)
    return "libbongo_items.dylib";
#else
    return "libbongo_items.so";
#endif
}
bool bongo_cat_items_managed_path(BongoCatApp *app, const char *suffix, char *out) {
    char directory[BONGO_CAT_PATH_CAP], name[80];
    snprintf(name, sizeof(name), "%s%s", bongo_cat_items_filename(), suffix);
    return app && bongo_cat_path_join(directory, sizeof(directory), app->data_root, "plugins") &&
        bongo_cat_path_join(out, BONGO_CAT_PATH_CAP, directory, name);
}
static bool resolve(BongoCatApp *app, char *out) {
    const char *override = SDL_getenv("BONGO_CAT_PLUGIN_DIRECTORY");
    if (override && override[0]) return bongo_cat_path_join(out, BONGO_CAT_PATH_CAP,
        override, bongo_cat_items_filename()) && bongo_cat_path_is_file(out);
    if (bongo_cat_items_managed_path(app, "", out) && bongo_cat_path_is_file(out)) return true;
    const char *base = SDL_GetBasePath(); /* Borrowed from SDL3. */
    char directory[BONGO_CAT_PATH_CAP];
    if (!base) return false;
    if (bongo_cat_path_join(directory, sizeof(directory), base, "plugins") &&
        bongo_cat_path_join(out, BONGO_CAT_PATH_CAP, directory, bongo_cat_items_filename()) &&
        bongo_cat_path_is_file(out)) return true;
#ifdef __APPLE__
    if (bongo_cat_path_join(directory, sizeof(directory), base, "../PlugIns") &&
        bongo_cat_path_join(out, BONGO_CAT_PATH_CAP, directory, bongo_cat_items_filename()) &&
        bongo_cat_path_is_file(out)) return true;
#endif
    return false;
}
void bongo_cat_items_info(BongoCatApp *app, BongoCatItemsInfo *info) {
    if (!info) return;
    *info = (BongoCatItemsInfo){0};
    if (!app) return;
    info->installed = resolve(app, info->path);
    char path[BONGO_CAT_PATH_CAP];
    info->enabled = info->installed && bongo_cat_items_managed_path(app, ".disabled", path) &&
        !bongo_cat_path_is_file(path);
    info->managed = info->installed && bongo_cat_items_managed_path(app, "", path) &&
        !strcmp(path, info->path);
    info->active = app->items != NULL;
}
const BongoCatItemsPlugin *bongo_cat_items_query(SDL_SharedObject *library) {
    union { SDL_FunctionPointer function; BongoCatItemsQuery query; } entry;
    entry.function = SDL_LoadFunction(library, BONGO_CAT_ITEMS_SYMBOL);
    const BongoCatItemsPlugin *api = entry.function ? entry.query(BONGO_CAT_ITEMS_ABI) : NULL;
    return api && api->abi_version == BONGO_CAT_ITEMS_ABI && api->struct_size == sizeof(*api) &&
        api->create && api->destroy && api->page && api->snapshot && api->restore &&
        api->add && api->update && api->remove ? api : NULL;
}
static void detach(BongoCatApp *app) {
    if (!app->items) return;
    bongo_cat_plugin_worker_stop(&app->items->worker);
    bongo_cat_overlay_set_items(app->overlay, NULL, NULL, 0);
    bongo_cat_items_release_graphics(app);
    BongoCatItems *v = app->items;
    app->items = NULL; /* Page disappears before library unload. */
    v->api->destroy(v->instance);
    SDL_UnloadObject(v->library);
    SDL_free(v);
    bongo_cat_preferences_invalidate(app->preferences);
    app->dirty = true;
}
static bool start(BongoCatApp *app, BongoCatError *error) {
    if (!app || app->secondary_pet) return false;
    if (app->items) return true;
    BongoCatItemsInfo info;
    bongo_cat_items_info(app, &info);
    if (!info.enabled) return false;
    SDL_SharedObject *library = SDL_LoadObject(info.path);
    const BongoCatItemsPlugin *api = library ? bongo_cat_items_query(library) : NULL;
    BongoCatItems *v = api ? SDL_calloc(1, sizeof(*v)) : NULL;
    if (v) {
        atomic_init(&v->unsaved, false); atomic_init(&v->loaded, false);
        v->library = library; v->api = api; v->instance = api->create();
        v->page.struct_size = sizeof(v->page);
    }
    bool valid = v && v->instance && api->page(v->instance, &v->page) &&
        v->page.struct_size == sizeof(v->page) &&
        memchr(v->page.id, 0, sizeof(v->page.id)) &&
        memchr(v->page.title_key, 0, sizeof(v->page.title_key)) &&
        memchr(v->page.title, 0, sizeof(v->page.title)) &&
        !strcmp(v->page.id, "items") && v->page.title_key[0] && v->page.title[0];
    if (!valid) {
        if (v && v->instance) api->destroy(v->instance);
        SDL_free(v);
        if (library) SDL_UnloadObject(library);
        bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT, "Items plugin ABI or page is incompatible");
        return false;
    }
    app->items = v;
    if (!bongo_cat_items_worker_start(app)) { detach(app); return false; }
    bongo_cat_preferences_invalidate(app->preferences);
    return true;
}
void bongo_cat_items_start(BongoCatApp *app) {
    BongoCatError error = {0};
    if (!start(app, &error) && error.message[0])
        SDL_LogWarn(BONGO_CAT_LOG_LIFECYCLE, "[items] %s", error.message);
}
void bongo_cat_items_stop(BongoCatApp *app) {
    if (!app || !app->items) return;
    detach(app);
}
static bool marker(BongoCatApp *app, bool enabled) {
    char path[BONGO_CAT_PATH_CAP], directory[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_path_join(directory, sizeof(directory), app->data_root, "plugins") ||
        !bongo_cat_path_create_directory(directory) ||
        !bongo_cat_items_managed_path(app, ".disabled", path)) return false;
    if (enabled) return !bongo_cat_path_is_file(path) || bongo_cat_file_remove(path);
    FILE *file = bongo_cat_file_open(path, "wb");
    return file && fclose(file) == 0;
}
bool bongo_cat_items_set_enabled(BongoCatApp *app, bool enabled, BongoCatError *error) {
    if (!app || app->secondary_pet) return false;
    if (!enabled && bongo_cat_items_busy(app)) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Items worker is busy; retry when the operation finishes"); return false;
    }
    if (!enabled && app->items && app->items->unsaved) {
        (void)bongo_cat_items_queue_save(app);
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Items save is queued; retry after it finishes"); return false;
    }
    if (!marker(app, enabled)) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Cannot save items plugin activation state");
        return false;
    }
    if (!enabled) { detach(app); return true; }
    if (start(app, error)) return true;
    (void)marker(app, false);
    return false;
}
const BongoCatItemsPage *bongo_cat_items_page(BongoCatApp *app) {
    return app && app->items ? &app->items->page : NULL;
}
bool bongo_cat_items_snapshot(BongoCatApp *app, BongoCatItemScene *scene) {
    if (!app || !app->items || !scene) return false;
    if (!atomic_load_explicit(&app->items->loaded, memory_order_acquire) &&
        app->items->worker.thread && SDL_GetCurrentThreadID() != SDL_GetThreadID(app->items->worker.thread)) return false;
    *scene = (BongoCatItemScene){0}; scene->struct_size = sizeof(*scene);
    if (app->items->worker.state_mutex) SDL_LockMutex(app->items->worker.state_mutex);
    bool ok = app->items->api->snapshot(app->items->instance, scene) != 0 &&
        scene->struct_size == sizeof(*scene) && scene->count <= BONGO_CAT_ITEMS_CAP;
    if (app->items->worker.state_mutex) SDL_UnlockMutex(app->items->worker.state_mutex);
    return ok;
}
void bongo_cat_items_changed(BongoCatApp *app) {
    if (!app || !app->items) return;
    app->items->unsaved = true;
    if (app->items->worker.thread && SDL_GetCurrentThreadID() == SDL_GetThreadID(app->items->worker.thread)) {
        atomic_store_explicit(&app->items->worker.changed, true, memory_order_release); return;
    }
    bongo_cat_items_publish(app);
}
void bongo_cat_items_publish(BongoCatApp *app) {
    ++app->items->revision;
    bongo_cat_overlay_set_items(app->overlay, bongo_cat_items_revision(app) ?
        bongo_cat_items_draw_native : NULL, app, app->items->revision);
    bongo_cat_window_snapshot_end(app);
    bongo_cat_window_mark_hit_dirty(app);
    app->dirty = true;
    bongo_cat_preferences_invalidate(app->preferences);
}
uint64_t bongo_cat_items_revision(BongoCatApp *app) {
    BongoCatItemScene scene;
    return bongo_cat_items_snapshot(app, &scene) && scene.count ? app->items->revision : 0;
}
bool bongo_cat_items_update(BongoCatApp *app, uint32_t index, const BongoCatItem *item) {
    if (!app || !app->items || bongo_cat_items_busy(app)) return false;
    SDL_LockMutex(app->items->worker.state_mutex);
    bool ok = app->items->api->update(app->items->instance, index, item) != 0;
    SDL_UnlockMutex(app->items->worker.state_mutex);
    if (!ok) return false;
    bongo_cat_items_changed(app);
    return true;
}
bool bongo_cat_items_duplicate(BongoCatApp *app, uint32_t index) {
    BongoCatItemScene scene;
    if (bongo_cat_items_busy(app) || !bongo_cat_items_snapshot(app, &scene) || index >= scene.count) return false;
    SDL_LockMutex(app->items->worker.state_mutex);
    bool ok = app->items->api->add(app->items->instance, &scene.items[index]) != 0;
    SDL_UnlockMutex(app->items->worker.state_mutex);
    if (!ok) return false;
    bongo_cat_items_changed(app);
    return true;
}
bool bongo_cat_items_remove(BongoCatApp *app, uint32_t index) {
    if (!app || !app->items || bongo_cat_items_busy(app)) return false;
    SDL_LockMutex(app->items->worker.state_mutex);
    bool ok = app->items->api->remove(app->items->instance, index) != 0;
    SDL_UnlockMutex(app->items->worker.state_mutex);
    if (!ok) return false;
    bongo_cat_items_release_graphics(app);
    bongo_cat_items_changed(app);
    return true;
}
