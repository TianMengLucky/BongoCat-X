#ifndef BONGO_CAT_IMAGE_ROWS_BRIDGE_H
#define BONGO_CAT_IMAGE_ROWS_BRIDGE_H
#include "image_internal.h"
#include "bongo_cat/safe_media.h"
typedef struct BongoCatRowBridge {
    BongoCatImageRows rows;
    void *userdata;
} BongoCatRowBridge;
bool bongo_cat_image_rows_bridge(void *userdata, unsigned char *pixels,
    int width, int rows, int height, int y);
#endif
