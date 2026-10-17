#include "plugin_models.h"
#include "bongo_cat/app.h"
#include "model_import.h"
#include "runtime.h"
#include <string.h>
static uint32_t query(void *context, const char *id) {
    BongoCatApp *app = ((BongoCatPluginModelsContext *)context)->app;
    const BongoCatModelEntry *model = bongo_cat_models_find(&app->models,id);
    if (!model || model->managed || !model->storage_directory[0]) return 0;
    return bongo_cat_settings_model_removed(&app->settings,id) ? 2u : 1u;
}
static uint32_t remove_model(void *context, const char *id) {
    BongoCatPluginModelsContext *value = context;
    return query(context,id) == 1 && bongo_cat_app_remove_model(value->app,id,value->error) == BONGO_CAT_OK;
}
static uint32_t rename_model(void *context, const char *id, const char *name) {
    if (!query(context,id) || !name || !name[0] || strlen(name) >= BONGO_CAT_ID_CAP) return 0;
    BongoCatApp *app = ((BongoCatPluginModelsContext *)context)->app;
    for (size_t i = 0; i < app->models.count; ++i) {
        BongoCatModelEntry *model = &app->models.entries[i];
        if (strcmp(model->id,id)) continue;
        char saved[BONGO_CAT_ID_CAP];
        if ((!bongo_cat_import_store_name_read(model->storage_directory,saved,sizeof(saved)) || strcmp(saved,name)) &&
            !bongo_cat_import_store_name_write(model->storage_directory,name)) return 0;
        if (!strcmp(model->display_name,name)) return 0;
        SDL_strlcpy(model->display_name,name,sizeof(model->display_name));
        return 1;
    }
    return 0;
}
BongoCatPluginModelsHost bongo_cat_plugin_models_host(BongoCatPluginModelsContext *context) {
    return (BongoCatPluginModelsHost){1,sizeof(BongoCatPluginModelsHost),context,query,remove_model,rename_model};
}
