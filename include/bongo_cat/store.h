#ifndef BONGO_CAT_STORE_H
#define BONGO_CAT_STORE_H
#include "bongo_cat/app.h"
#include "bongo_cat/store_plugin.h"
typedef struct BongoCatStoreInfo {
    bool installed, enabled, active, managed;
    char path[BONGO_CAT_PATH_CAP];
} BongoCatStoreInfo;
/* Status: idle=0, loading=1, ready=2, unpublished=3, failure=4. */
void bongo_cat_store_start(BongoCatApp *app);
void bongo_cat_store_stop(BongoCatApp *app);
void bongo_cat_store_tick(BongoCatApp *app);
bool bongo_cat_store_active(BongoCatApp *app);
bool bongo_cat_store_busy(BongoCatApp *app);
void bongo_cat_store_info(BongoCatApp *app, BongoCatStoreInfo *info);
bool bongo_cat_store_set_enabled(BongoCatApp *app, bool enabled, BongoCatError *error);
bool bongo_cat_store_refresh(BongoCatApp *app);
unsigned bongo_cat_store_status(BongoCatApp *app, unsigned *count);
bool bongo_cat_store_entry(BongoCatApp *app, unsigned index, BongoCatStoreEntry *out);
bool bongo_cat_store_import(BongoCatApp *app, const char *path, bool *matched, BongoCatError *error);
bool bongo_cat_store_remove(BongoCatApp *app, BongoCatError *error);
bool bongo_cat_store_preview(BongoCatApp *app, unsigned index, char *path);
bool bongo_cat_store_download(BongoCatApp *app, unsigned index);
bool bongo_cat_store_installed(BongoCatApp *app, const BongoCatStoreEntry *entry);
bool bongo_cat_store_uninstall(BongoCatApp *app, const BongoCatStoreEntry *entry, BongoCatError *error);
unsigned bongo_cat_store_operation(BongoCatApp *app, unsigned *index, uint64_t *received, uint64_t *total);
const char *bongo_cat_store_filename(void);
#endif
