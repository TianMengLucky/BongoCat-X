#ifndef BONGO_CAT_MODEL_PLUGINS_H
#define BONGO_CAT_MODEL_PLUGINS_H
#include "bongo_cat/model_plugin.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct BongoCatModelPluginInfo {
    bool installed, enabled, active, managed;
    char path[BONGO_CAT_PATH_CAP];
} BongoCatModelPluginInfo;
void bongo_cat_model_plugins_set_root(const char *data_root);
const char *bongo_cat_model_plugin_filename(BongoCatModelEngine engine);
bool bongo_cat_model_plugin_resolve(BongoCatModelEngine engine, char *path, size_t capacity);
void bongo_cat_model_plugin_info(BongoCatModelEngine engine, BongoCatModelPluginInfo *info);
bool bongo_cat_model_plugin_set_enabled(BongoCatModelEngine engine, bool enabled, BongoCatError *error);
bool bongo_cat_model_plugin_install(const char *source, BongoCatModelEngine *engine, BongoCatError *error);
bool bongo_cat_model_plugin_install_archive(const char *source, const char *data_root, BongoCatModelEngine *engine, BongoCatError *error);
bool bongo_cat_model_plugin_remove(BongoCatModelEngine engine, BongoCatError *error);
void bongo_cat_model_plugin_retain(BongoCatModelEngine engine);
void bongo_cat_model_plugin_release(BongoCatModelEngine engine);
bool bongo_cat_model_plugin_descriptor_valid(const BongoCatModelPlugin *plugin, BongoCatModelEngine engine);
#ifdef __cplusplus
}
#endif
#endif
