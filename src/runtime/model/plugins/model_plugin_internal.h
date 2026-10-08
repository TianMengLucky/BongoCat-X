#ifndef BONGO_CAT_MODEL_PLUGIN_INTERNAL_H
#define BONGO_CAT_MODEL_PLUGIN_INTERNAL_H
#include "bongo_cat/model_plugin_host.h"
struct BongoCatModelRuntime {
    const BongoCatModelPluginOps *ops;
    BongoCatModelRuntime *instance;
    SDL_SharedObject *library;
    BongoCatModelEngine engine;
    BongoCatRhiDeviceInfo rhi;
    int width, height;
    char asset_root[BONGO_CAT_PATH_CAP];
};
extern const BongoCatModelPluginOps bongo_cat_model_stub_ops;
bool bongo_cat_model_plugin_open(BongoCatModelRuntime *target,
    BongoCatModelEngine engine, BongoCatError *error);
void bongo_cat_model_plugin_close(BongoCatModelRuntime *runtime);
#endif
