#ifndef BONGO_CAT_ITEMS_H
#define BONGO_CAT_ITEMS_H
#include "bongo_cat/common.h"
#include "bongo_cat/items_plugin.h"
typedef struct BongoCatApp BongoCatApp;
typedef struct BongoCatItems BongoCatItems;
typedef struct SDL_Renderer SDL_Renderer;
typedef struct BongoCatItemsInfo {
    bool installed, enabled, active, managed;
    char path[BONGO_CAT_PATH_CAP];
} BongoCatItemsInfo;
const char *bongo_cat_items_filename(void);
void bongo_cat_items_info(BongoCatApp *app, BongoCatItemsInfo *info);
void bongo_cat_items_start(BongoCatApp *app);
void bongo_cat_items_stop(BongoCatApp *app);
bool bongo_cat_items_set_enabled(BongoCatApp *app, bool enabled, BongoCatError *error);
bool bongo_cat_items_import_plugin(BongoCatApp *app, const char *path, bool *matched, BongoCatError *error);
bool bongo_cat_items_remove_plugin(BongoCatApp *app, BongoCatError *error);
const BongoCatItemsPage *bongo_cat_items_page(BongoCatApp *app);
bool bongo_cat_items_snapshot(BongoCatApp *app, BongoCatItemScene *scene);
bool bongo_cat_items_import_image(BongoCatApp *app, const char *path, BongoCatError *error);
bool bongo_cat_items_update(BongoCatApp *app, uint32_t index, const BongoCatItem *item);
bool bongo_cat_items_duplicate(BongoCatApp *app, uint32_t index);
bool bongo_cat_items_remove(BongoCatApp *app, uint32_t index);
bool bongo_cat_items_save(BongoCatApp *app, BongoCatError *error);
void bongo_cat_items_release_graphics(BongoCatApp *app);
void bongo_cat_items_draw_gl(BongoCatApp *app, bool front, int width, int height,
    int x, int y, int cw, int ch);
bool bongo_cat_items_draw_native(void *app, SDL_Renderer *renderer, bool front,
    int width, int height, int x, int y, int cw, int ch);
bool bongo_cat_items_queue_import(BongoCatApp *app, const char *path);
bool bongo_cat_items_queue_save(BongoCatApp *app);
bool bongo_cat_items_busy(BongoCatApp *app);
void bongo_cat_items_tick(BongoCatApp *app);
uint64_t bongo_cat_items_revision(BongoCatApp *app);
#endif
