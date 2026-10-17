#include "image_rows_bridge.h"
#include <limits.h>

bool bongo_cat_image_rows_bridge(void *userdata, unsigned char *pixels,
    int width, int count, int height, int y) {
    BongoCatRowBridge *bridge = userdata;
    BongoCatImage rows = {.pixels = pixels, .width = width, .height = count};
    return bridge->rows(bridge->userdata, &rows, height, y);
}

bool bongo_cat_image_decode_png_rows(const char *path,
    BongoCatImageRows consume, void *consumer,
    BongoCatImageProgress progress, void *userdata) {
    return bongo_cat_image_decode_png_rows_cancellable(path, consume, consumer,
        progress, userdata, NULL, NULL);
}

bool bongo_cat_image_decode_png_rows_cancellable(const char *path,
    BongoCatImageRows consume, void *consumer,
    BongoCatImageProgress progress, void *userdata,
    BongoCatImageCancelled cancelled, void *cancel_data) {
    if (!path || !consume) return false;
    BongoCatRowBridge bridge = {consume, consumer};
    BongoSafeRowCallbacks callbacks = {bongo_cat_image_rows_bridge, &bridge,
        progress, userdata, cancelled, cancel_data};
    return bongo_safe_png_rows(path, INT_MAX, INT_MAX, callbacks) == BONGO_CAT_OK;
}
