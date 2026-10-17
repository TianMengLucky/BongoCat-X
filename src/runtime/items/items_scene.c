#include "items_internal.h"
#include "bongo_cat/path.h"
static bool execute(BongoCatApp *app, uint32_t operation, const char *path, BongoCatError *error) {
    char directory[BONGO_CAT_PATH_CAP];
    if (!app || !app->items || !app->items->io ||
        !bongo_cat_path_join(directory, sizeof(directory), app->data_root, "items")) return false;
    BongoCatItems *v = app->items;
    BongoCatPluginIoContext context = {v->worker.state_mutex, NULL};
    BongoCatPluginIoHost host = bongo_cat_plugin_io_host(&context);
    if (v->io->execute(v->instance, directory, &host, operation, 0, NULL, path, 0)) return true;
    bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Items plugin operation failed; saved scene is preserved");
    return false;
}
bool bongo_cat_items_load_scene(BongoCatApp *app, BongoCatError *error) {
    return execute(app, BONGO_PLUGIN_IO_LOAD, NULL, error);
}
bool bongo_cat_items_save(BongoCatApp *app, BongoCatError *error) {
    if (!execute(app, BONGO_PLUGIN_IO_SAVE, NULL, error)) return false;
    app->items->unsaved = false;
    return true;
}
bool bongo_cat_items_import_image(BongoCatApp *app, const char *path, BongoCatError *error) {
    if (!execute(app, BONGO_PLUGIN_IO_IMPORT, path, error)) return false;
    bongo_cat_items_changed(app);
    app->items->unsaved = false;
    return true;
}
