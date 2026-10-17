#ifndef BONGO_CAT_RUNTIME_PLUGIN_MODELS_H
#define BONGO_CAT_RUNTIME_PLUGIN_MODELS_H
#include "bongo_cat/plugin_models.h"
#include "bongo_cat/common.h"
typedef struct BongoCatApp BongoCatApp;
typedef struct BongoCatPluginModelsContext {
    BongoCatApp *app;
    BongoCatError *error;
} BongoCatPluginModelsContext;
BongoCatPluginModelsHost bongo_cat_plugin_models_host(BongoCatPluginModelsContext *);
#endif
