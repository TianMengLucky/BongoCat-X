#include "bongo_cat/model_plugins.h"
#include "bongo_cat/model_plugin_host.h"
#include "bongo_cat/path.h"
#include "bongo_cat/file.h"
#include "model_import_archive.h"
#include <SDL3/SDL.h>
#include <stdio.h>
static char managed_root[BONGO_CAT_PATH_CAP];
static char data_root[BONGO_CAT_PATH_CAP];
static unsigned active[3]; /* Main-thread-only plugin lifetime counts. */
void bongo_cat_model_plugins_set_root(const char *root) {
    managed_root[0] = '\0';
    SDL_strlcpy(data_root, root ? root : "", sizeof(data_root));
    if (root) (void)bongo_cat_path_join(managed_root, sizeof(managed_root), root, "plugins");
}
const char *bongo_cat_model_plugin_filename(BongoCatModelEngine engine) {
#ifdef _WIN32
    return engine == BONGO_CAT_MODEL_ENGINE_LIVE2D ? "bongo_live2d.dll" : "bongo_inox2d.dll";
#elif defined(__APPLE__)
    return engine == BONGO_CAT_MODEL_ENGINE_LIVE2D ? "libbongo_live2d.dylib" : "libbongo_inox2d.dylib";
#else
    return engine == BONGO_CAT_MODEL_ENGINE_LIVE2D ? "libbongo_live2d.so" : "libbongo_inox2d.so";
#endif
}
static bool managed_path(BongoCatModelEngine engine, const char *suffix, char *path, size_t cap) {
    char name[80];
    snprintf(name, sizeof(name), "%s%s", bongo_cat_model_plugin_filename(engine), suffix);
    return managed_root[0] && bongo_cat_path_join(path, cap, managed_root, name);
}
static bool enabled(BongoCatModelEngine engine) {
    char path[BONGO_CAT_PATH_CAP];
    return !managed_path(engine, ".disabled", path, sizeof(path)) || !bongo_cat_path_is_file(path);
}
bool bongo_cat_model_plugin_resolve(BongoCatModelEngine engine, char *path, size_t cap) {
    const char *name = bongo_cat_model_plugin_filename(engine);
    const char *override = SDL_getenv("BONGO_CAT_PLUGIN_DIRECTORY");
    if (override && override[0])
        return bongo_cat_path_join(path, cap, override, name) && bongo_cat_path_is_file(path);
    if (managed_path(engine, "", path, cap) && bongo_cat_path_is_file(path)) return true;
    const char *base = SDL_GetBasePath(); /* SDL-owned. */
    char directory[BONGO_CAT_PATH_CAP];
    if (!base) return false;
    if (bongo_cat_path_join(directory, sizeof(directory), base, "plugins") &&
        bongo_cat_path_join(path, cap, directory, name) && bongo_cat_path_is_file(path)) return true;
#ifdef __APPLE__
    if (bongo_cat_path_join(directory, sizeof(directory), base, "../PlugIns") &&
        bongo_cat_path_join(path, cap, directory, name) && bongo_cat_path_is_file(path)) return true;
#endif
    path[0] = '\0'; return false;
}
void bongo_cat_model_plugin_info(BongoCatModelEngine engine, BongoCatModelPluginInfo *info) {
    if (!info) return;
    *info = (BongoCatModelPluginInfo){0};
    info->installed = bongo_cat_model_plugin_resolve(engine, info->path, sizeof(info->path));
    info->enabled = info->installed && enabled(engine);
    info->active = engine > 0 && engine < 3 && active[engine] > 0;
    char managed[BONGO_CAT_PATH_CAP];
    info->managed = info->installed && managed_path(engine, "", managed, sizeof(managed)) && !SDL_strcmp(managed, info->path);
}
void bongo_cat_model_plugin_retain(BongoCatModelEngine engine) { if (engine > 0 && engine < 3) ++active[engine]; }
void bongo_cat_model_plugin_release(BongoCatModelEngine engine) { if (engine > 0 && engine < 3 && active[engine]) --active[engine]; }
static bool mutable_plugin(BongoCatModelEngine engine, BongoCatError *error) {
    if (engine <= 0 || engine >= 3 || active[engine]) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_ARGUMENT, "Switch to another renderer before changing an active plugin"); return false;
    }
    if (!managed_root[0] || !bongo_cat_path_create_directory(managed_root)) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Cannot create the plugin directory"); return false;
    }
    return true;
}
bool bongo_cat_model_plugin_set_enabled(BongoCatModelEngine engine, bool value, BongoCatError *error) {
    if (!mutable_plugin(engine, error)) return false;
    char path[BONGO_CAT_PATH_CAP];
    if (!managed_path(engine, ".disabled", path, sizeof(path))) return false;
    bool ok = true;
    if (value) ok = !bongo_cat_path_is_file(path) || bongo_cat_file_remove(path);
    else { FILE *file = bongo_cat_file_open(path, "wb"); ok = file && fclose(file) == 0; }
    if (!ok) bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Cannot save the plugin activation state");
    return ok;
}
bool bongo_cat_model_plugin_descriptor_valid(const BongoCatModelPlugin *p, BongoCatModelEngine engine) {
    return p && p->abi_version == BONGO_CAT_MODEL_PLUGIN_ABI && p->struct_size == sizeof(*p) &&
        p->engine >= 1 && p->engine <= 2 && (!engine || p->engine == (uint32_t)engine) && p->name &&
        p->ops.create && p->ops.destroy && p->ops.load_ex && p->ops.ready && p->ops.update &&
        p->ops.draw_checked && p->ops.resize && p->ops.set_rhi_info;
}
bool bongo_cat_model_plugin_install(const char *source, BongoCatModelEngine *engine, BongoCatError *error) {
    uint64_t size = 0;
    if (!source || !bongo_cat_path_file_size(source, &size) || size > 256u * 1024u * 1024u) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_ARGUMENT, "Invalid plugin file"); return false;
    }
    if (bongo_cat_import_is_archive(source))
        return bongo_cat_model_plugin_install_archive(source, data_root, engine, error);
    SDL_SharedObject *library = SDL_LoadObject(source);
    if (!library) { bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM, "Cannot load plugin: %s", SDL_GetError()); return false; }
    union { SDL_FunctionPointer function; BongoCatModelPluginQuery query; } entry;
    entry.function = SDL_LoadFunction(library, BONGO_CAT_MODEL_PLUGIN_SYMBOL);
    const BongoCatModelPlugin *p = entry.function ? entry.query(BONGO_CAT_MODEL_PLUGIN_ABI, bongo_cat_model_plugin_host()) : NULL;
    BongoCatModelEngine kind = bongo_cat_model_plugin_descriptor_valid(p, 0) ? (BongoCatModelEngine)p->engine : 0;
    SDL_UnloadObject(library);
    if (!kind) { bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT, "Plugin ABI or renderer type is incompatible"); return false; }
    if (!mutable_plugin(kind, error)) return false;
    char temporary[BONGO_CAT_PATH_CAP], target[BONGO_CAT_PATH_CAP];
    if (!managed_path(kind, ".new", temporary, sizeof(temporary)) || !managed_path(kind, "", target, sizeof(target))) return false;
    bool ok = bongo_cat_path_copy_file(source, temporary) && bongo_cat_file_replace(temporary, target, true);
    if (!ok) { (void)bongo_cat_file_remove(temporary); bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Cannot install the plugin"); return false; }
    if (engine) *engine = kind;
    return bongo_cat_model_plugin_set_enabled(kind, true, error);
}
bool bongo_cat_model_plugin_remove(BongoCatModelEngine engine, BongoCatError *error) {
    if (!mutable_plugin(engine, error)) return false;
    char path[BONGO_CAT_PATH_CAP];
    if (!managed_path(engine, "", path, sizeof(path))) return false;
    if (bongo_cat_path_is_file(path) && !bongo_cat_file_remove(path)) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Cannot remove the plugin"); return false;
    }
    return bongo_cat_model_plugin_set_enabled(engine, false, error);
}
