#include "model_plugin_internal.h"
#include "bongo_cat/path.h"
#include "bongo_cat/log.h"
#include "bongo_cat/model_plugins.h"
#include <stdio.h>
#include <string.h>

bool bongo_cat_model_plugin_open(BongoCatModelRuntime *target,
    BongoCatModelEngine engine, BongoCatError *error) {
    BongoCatModelPluginInfo status;
    bongo_cat_model_plugin_info(engine, &status);
    if (!status.enabled) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM, "Renderer plugin is disabled"); return false;
    }
    SDL_SharedObject *library = status.installed ? SDL_LoadObject(status.path) : NULL;
    if (!library) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "Model plugin is unavailable: %s (%s)", bongo_cat_model_plugin_filename(engine), SDL_GetError());
        return false;
    }
    union { SDL_FunctionPointer function; BongoCatModelPluginQuery query; } entry;
    entry.function = SDL_LoadFunction(library, BONGO_CAT_MODEL_PLUGIN_SYMBOL);
    const BongoCatModelPlugin *plugin = entry.function ?
        entry.query(BONGO_CAT_MODEL_PLUGIN_ABI, bongo_cat_model_plugin_host()) : NULL;
    if (!bongo_cat_model_plugin_descriptor_valid(plugin, engine)) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "Model plugin ABI or engine does not match this application");
        SDL_UnloadObject(library);
        return false;
    }
    if (!(plugin->graphics_backends & (1u << (unsigned)target->rhi.backend))) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "%s does not support the selected graphics backend", plugin->name);
        SDL_UnloadObject(library);
        return false;
    }
    if ((plugin->flags & BONGO_CAT_PLUGIN_REQUIRES_CORE) &&
        !bongo_cat_platform_live2d_core_available()) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_CUBISM,
            "Live2D plugin requires the user-supplied Cubism Core");
        SDL_UnloadObject(library);
        return false;
    }
    BongoCatModelRuntime *instance = plugin->ops.create(target->asset_root, error);
    if (!instance) { SDL_UnloadObject(library); return false; }
    target->ops = &plugin->ops;
    target->instance = instance;
    target->library = library;
    target->engine = engine;
    bongo_cat_model_plugin_retain(engine);
    target->ops->set_rhi_info(instance, &target->rhi);
    target->ops->resize(instance, target->width, target->height);
    SDL_LogInfo(BONGO_CAT_LOG_LIFECYCLE, "[model] loaded plugin: %s", plugin->name);
    return true;
}

void bongo_cat_model_plugin_close(BongoCatModelRuntime *runtime) {
    if (runtime->instance) runtime->ops->destroy(runtime->instance);
    if (runtime->library) { SDL_UnloadObject(runtime->library); bongo_cat_model_plugin_release(runtime->engine); }
    runtime->instance = NULL;
    runtime->library = NULL;
}
