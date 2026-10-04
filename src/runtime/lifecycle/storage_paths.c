#include "storage_paths.h"
#include "portable_mode.h"
#include "runtime.h"

#include "bongo_cat/file.h"
#include "bongo_cat/path.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include "windows_package.h"
#endif

static bool child_path(char *target, size_t capacity, const char *root,
    const char *child) {
    return root && root[0] &&
        bongo_cat_path_join(target, capacity, root, child);
}

static bool writable_directory(const char *root) {
    char probe[BONGO_CAT_PATH_CAP];
    if (!root || !root[0] || !bongo_cat_path_create_directory(root) ||
        !bongo_cat_path_join(probe, sizeof(probe), root,
            ".bongocat-write-probe")) return false;
    FILE *file = bongo_cat_file_open(probe, "wb");
    bool written = file && fputc('1', file) != EOF;
    if (file && fclose(file) != 0) written = false;
    bongo_cat_file_remove(probe);
    return written;
}

static bool roots_under(BongoCatApp *app, const char *root) {
    return child_path(app->config_root, sizeof(app->config_root),
            root, "config") &&
        child_path(app->data_root, sizeof(app->data_root),
            root, "data") &&
        child_path(app->models_root, sizeof(app->models_root),
            root, "models") &&
        child_path(app->cache_root, sizeof(app->cache_root),
            root, "cache") &&
        child_path(app->state_root, sizeof(app->state_root),
            root, "state") &&
        child_path(app->log_root, sizeof(app->log_root),
            root, "logs");
}

static bool isolated_roots(BongoCatApp *app) {
    return roots_under(app, app->storage_root);
}

#ifdef _WIN32
static bool platform_roots(BongoCatApp *app) {
    char local_app[BONGO_CAT_PATH_CAP];
    if (bongo_cat_windows_is_packaged())
        return bongo_cat_windows_package_storage_root(local_app,
            sizeof(local_app)) && roots_under(app, local_app);
    const char *local = SDL_getenv("LOCALAPPDATA");
    return child_path(local_app, sizeof(local_app), local, BONGO_CAT_NAME) &&
        roots_under(app, local_app);
}
#elif defined(__APPLE__)
static bool platform_roots(BongoCatApp *app) {
    const char *home = SDL_GetUserFolder(SDL_FOLDER_HOME);
    char library[BONGO_CAT_PATH_CAP], support[BONGO_CAT_PATH_CAP];
    char app_support[BONGO_CAT_PATH_CAP];
    return child_path(library, sizeof(library), home, "Library") &&
        child_path(support, sizeof(support), library, "Application Support") &&
        child_path(app_support, sizeof(app_support), support, BONGO_CAT_NAME) &&
        child_path(app->config_root, sizeof(app->config_root),
            app_support, "config") &&
        child_path(app->data_root, sizeof(app->data_root), app_support, "data") &&
        child_path(app->models_root, sizeof(app->models_root),
            app_support, "models") &&
        child_path(app->state_root, sizeof(app->state_root), app_support, "state") &&
        child_path(app->cache_root, sizeof(app->cache_root),
            library, "Caches/BongoCat") &&
        child_path(app->log_root, sizeof(app->log_root),
            library, "Logs/BongoCat");
}
#else
static bool xdg_root(char *target, size_t capacity, const char *variable,
    const char *home_suffix) {
    const char *configured = SDL_getenv(variable);
    if (configured && configured[0])
        return child_path(target, capacity, configured, "bongocat");
    const char *home = SDL_GetUserFolder(SDL_FOLDER_HOME);
    char fallback[BONGO_CAT_PATH_CAP];
    return child_path(fallback, sizeof(fallback), home, home_suffix) &&
        child_path(target, capacity, fallback, "bongocat");
}

static bool platform_roots(BongoCatApp *app) {
    if (!xdg_root(app->config_root, sizeof(app->config_root),
            "XDG_CONFIG_HOME", ".config") ||
        !xdg_root(app->data_root, sizeof(app->data_root),
            "XDG_DATA_HOME", ".local/share") ||
        !xdg_root(app->state_root, sizeof(app->state_root),
            "XDG_STATE_HOME", ".local/state") ||
        !xdg_root(app->cache_root, sizeof(app->cache_root),
            "XDG_CACHE_HOME", ".cache")) return false;
    if (!child_path(app->models_root, sizeof(app->models_root),
        app->data_root, "models")) return false;
    return child_path(app->log_root, sizeof(app->log_root),
        app->state_root, "logs");
}
#endif

bool bongo_cat_storage_paths_prepare(BongoCatApp *app,
    BongoCatError *error) {
    if (!app) return false;
    /* Portable marker beside the executable; an explicit --storage-root
       argument (set earlier) always wins over it. */
    if (!app->storage_root[0] && bongo_cat_portable_mode_active() &&
        bongo_cat_portable_root(app->storage_root,
            sizeof(app->storage_root)))
        SDL_LogInfo(SDL_LOG_CATEGORY_CUSTOM,
            "Portable mode: storage root %s", app->storage_root);
    bool resolved = app->storage_root[0]
        ? isolated_roots(app) : platform_roots(app);
    if (resolved) {
        snprintf(app->primary_state_root, sizeof(app->primary_state_root),
            "%s", app->state_root);
    }
    if (resolved && app->secondary_pet) {
        resolved = bongo_cat_multi_pet_state_directory(app->state_root,
            sizeof(app->state_root), app->primary_state_root,
            app->secondary_model_id);
    }
    const char *roots[] = {app->config_root, app->data_root, app->models_root,
        app->cache_root, app->state_root, app->log_root};
    for (size_t i = 0; resolved && i < sizeof(roots) / sizeof(roots[0]); ++i)
        resolved = writable_directory(roots[i]);
    resolved = resolved && child_path(app->settings_path,
        sizeof(app->settings_path), app->config_root, "settings.json") &&
        child_path(app->session_path, sizeof(app->session_path),
            app->state_root, "session.json");
    if (resolved) return true;
    app->settings_path[0] = app->session_path[0] = '\0';
    bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
        "Application storage directories are unavailable or not writable");
    return false;
}
