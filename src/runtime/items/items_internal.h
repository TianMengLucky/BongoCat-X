#ifndef BONGO_CAT_ITEMS_INTERNAL_H
#define BONGO_CAT_ITEMS_INTERNAL_H
#include "plugin_page.h"
#include "plugin_events.h"
#include "bongo_cat/app.h"
#include "bongo_cat/items.h"
#include "bongo_cat/image.h"
#include "bongo_cat/gl_api.h"
#include <SDL3/SDL.h>
#include "plugin_worker.h"
#include "plugin_io.h"
#include "bongo_cat/items_render_plugin.h"
struct BongoCatItems {
    SDL_SharedObject *library;
    const BongoCatPluginEvents *events;
    BongoCatPluginPageSession page_ui;
    const BongoCatItemsPlugin *api;
    const BongoCatPluginIo *io;
    const BongoCatItemsRenderPlugin *render;
    const BongoCatPluginUiContribution *ui;
    void *instance;
    BongoCatItemsPage page;
    uint64_t revision;
    atomic_bool unsaved, loaded;
    BongoCatPluginWorker worker;
    BongoCatGL gl;
    GLuint program, vao, vbo;
    struct { char asset[80]; BongoCatImage image; GLuint texture; } images[BONGO_CAT_ITEMS_CAP];
};
bool bongo_cat_items_managed_path(BongoCatApp *app, const char *suffix, char *out);
const BongoCatItemsPlugin *bongo_cat_items_query(SDL_SharedObject *library);
bool bongo_cat_items_load_scene(BongoCatApp *app, BongoCatError *error);
void bongo_cat_items_changed(BongoCatApp *app);
void bongo_cat_items_publish(BongoCatApp *app);
void bongo_cat_items_prune_graphics(BongoCatApp *app);
bool bongo_cat_items_worker_start(BongoCatApp *app);
const BongoCatItemsRenderPlugin *bongo_cat_items_render_query(SDL_SharedObject *library);
#endif
