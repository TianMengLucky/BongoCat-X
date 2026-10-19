#ifndef BONGO_CAT_LAN_NATIVE_H
#define BONGO_CAT_LAN_NATIVE_H
#include "lan_surface.h"
typedef struct BongoCatLanNative BongoCatLanNative;
void bongo_cat_lan_native_destroy(BongoCatLanNative **);
bool bongo_cat_lan_native_update(BongoCatApp *, BongoCatLanNative **,
    const BongoCatLanSnapshot *, const char *);
SDL_Window *bongo_cat_lan_native_window(BongoCatLanNative *);
#endif
