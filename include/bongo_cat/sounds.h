#ifndef BONGO_CAT_SOUNDS_H
#define BONGO_CAT_SOUNDS_H
#include "bongo_cat/common.h"
#include "bongo_cat/sounds_plugin.h"
#include "bongo_cat/input.h"
typedef struct BongoCatApp BongoCatApp;
typedef struct BongoCatSoundsInfo { bool installed, enabled, active, managed; char path[BONGO_CAT_PATH_CAP]; } BongoCatSoundsInfo;
const char *bongo_cat_sounds_filename(void);
void bongo_cat_sounds_info(BongoCatApp *app, BongoCatSoundsInfo *info);
void bongo_cat_sounds_start(BongoCatApp *app);
void bongo_cat_sounds_stop(BongoCatApp *app);
bool bongo_cat_sounds_set_enabled(BongoCatApp *app, bool enabled, BongoCatError *error);
bool bongo_cat_sounds_import_plugin(BongoCatApp *app, const char *path, bool *matched, BongoCatError *error);
bool bongo_cat_sounds_remove_plugin(BongoCatApp *app, BongoCatError *error);
const BongoCatSoundsPage *bongo_cat_sounds_page(BongoCatApp *app);
bool bongo_cat_sounds_snapshot(BongoCatApp *app, BongoCatSoundScene *scene);
bool bongo_cat_sounds_key(BongoCatApp *app, uint32_t index, BongoCatSoundKey *key);
bool bongo_cat_sounds_import_audio(BongoCatApp *app, const char *key, const char *path, BongoCatError *error);
bool bongo_cat_sounds_clear(BongoCatApp *app, uint32_t index, BongoCatError *error);
bool bongo_cat_sounds_preview(BongoCatApp *app, uint32_t index, BongoCatError *error);
void bongo_cat_sounds_input(BongoCatApp *app, const BongoCatInputEvent *event, bool play);
bool bongo_cat_sounds_busy(BongoCatApp *app);
void bongo_cat_sounds_tick(BongoCatApp *app);
#endif
