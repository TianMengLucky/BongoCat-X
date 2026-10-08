#include "model_plugin_internal.h"
#include "bongo_cat/log.h"
#include "bongo_cat/path.h"
#include "bongo_cat/safe_ffi.h"
#include <stdlib.h>
#include <string.h>

BongoCatModelEngine bongo_cat_model_runtime_engine(const BongoCatModelRuntime *runtime) {
    return runtime ? runtime->engine : BONGO_CAT_MODEL_ENGINE_LIVE2D;
}
bool bongo_cat_model_runtime_rendering(const BongoCatModelRuntime *runtime) {
    return runtime && runtime->library && runtime->ops->ready(runtime->instance);
}
BongoCatModelRuntime *bongo_cat_model_runtime_create(const char *asset_root, BongoCatError *error) {
    BongoCatModelRuntime *runtime = calloc(1, sizeof(*runtime));
    if (!runtime) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_MEMORY, "Cannot allocate model runtime");
        return NULL;
    }
    runtime->ops = &bongo_cat_model_stub_ops;
    runtime->engine = BONGO_CAT_MODEL_ENGINE_LIVE2D;
    runtime->width = 612; runtime->height = 354;
    SDL_strlcpy(runtime->asset_root, asset_root ? asset_root : "", sizeof(runtime->asset_root));
    runtime->instance = runtime->ops->create(asset_root, error);
    if (!runtime->instance) { free(runtime); return NULL; }
    return runtime;
}
void bongo_cat_model_runtime_destroy(BongoCatModelRuntime *runtime) {
    if (!runtime) return;
    bongo_cat_model_plugin_close(runtime);
    free(runtime);
}
void bongo_cat_model_runtime_set_rhi_info(BongoCatModelRuntime *runtime,
    const BongoCatRhiDeviceInfo *info) {
    if (!runtime || !info) return;
    runtime->rhi = *info;
    if (runtime->ops->set_rhi_info) runtime->ops->set_rhi_info(runtime->instance, info);
}
BongoCatResult bongo_cat_model_runtime_load_ex(BongoCatModelRuntime *runtime,
    const char *directory, const char *setting, bool preset,
    const BongoCatModelRuntimeRenderOptions *options,
    const BongoCatModelRuntimeTextureOptions *textures,
    BongoCatModelRuntimeLoadProgress progress, void *userdata, BongoCatError *error) {
    if (!runtime || !directory || !setting) return BONGO_CAT_ERROR_ARGUMENT;
    char path[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_path_join(path, sizeof(path), directory, setting)) return BONGO_CAT_ERROR_ARGUMENT;
    int kind = bongo_safe_model_kind_file(path);
    if (!kind) { bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT, "Unknown model format"); return BONGO_CAT_ERROR_FORMAT; }
    BongoCatModelEngine engine = kind == 2 ? BONGO_CAT_MODEL_ENGINE_INOX2D : BONGO_CAT_MODEL_ENGINE_LIVE2D;
    if (runtime->library && runtime->engine == engine)
        return runtime->ops->load_ex(runtime->instance, directory, setting, preset,
            options, textures, progress, userdata, error);
    BongoCatModelRuntime replacement = *runtime;
    replacement.instance = NULL; replacement.library = NULL;
    BongoCatError optional = {0};
    if (!bongo_cat_model_plugin_open(&replacement, engine, &optional)) {
        if (engine != BONGO_CAT_MODEL_ENGINE_LIVE2D || runtime->library) {
            if (error) *error = optional;
            return optional.code ? optional.code : BONGO_CAT_ERROR_PLATFORM;
        }
        SDL_LogWarn(BONGO_CAT_LOG_LIFECYCLE, "%s; using diagnostic model backend", optional.message);
        return runtime->ops->load_ex(runtime->instance, directory, setting, preset,
            options, textures, progress, userdata, error);
    }
    BongoCatResult result = replacement.ops->load_ex(replacement.instance,
        directory, setting, preset, options, textures, progress, userdata, error);
    if (result != BONGO_CAT_OK) {
        bongo_cat_model_plugin_close(&replacement);
        return result;
    }
    /* Commit only after the new engine loaded successfully; failure keeps the old pet. */
    bongo_cat_model_plugin_close(runtime);
    *runtime = replacement;
    return BONGO_CAT_OK;
}
BongoCatResult bongo_cat_model_runtime_load(BongoCatModelRuntime *runtime,
    const char *directory, const char *setting, bool preset,
    const BongoCatModelRuntimeRenderOptions *options,
    BongoCatModelRuntimeLoadProgress progress, void *userdata, BongoCatError *error) {
    return bongo_cat_model_runtime_load_ex(runtime, directory, setting, preset,
        options, NULL, progress, userdata, error);
}
