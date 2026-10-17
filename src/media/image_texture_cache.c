#include "image_rows_bridge.h"
#include "image_texture_cache.h"

BongoCatResult bongo_cat_image_decode_cached_scaled_rows(const char *path,
    const char *digest, int max_width, int max_height,
    BongoCatImageRows consume, void *consumer, BongoCatImageProgress progress,
    void *userdata, BongoCatImageCancelled cancelled, void *cancel_data,
    BongoCatError *error) {
    if (!path || !consume || max_width < 1 || max_height < 1)
        return BONGO_CAT_ERROR_ARGUMENT;
    BongoCatRowBridge bridge = {consume, consumer};
    BongoSafeRowCallbacks callbacks = {bongo_cat_image_rows_bridge, &bridge,
        progress, userdata, cancelled, cancel_data};
    BongoCatResult result = (BongoCatResult)bongo_safe_cached_png_rows(path, digest, max_width, max_height, callbacks);
    if (result != BONGO_CAT_OK)
        bongo_cat_error_set(error, result, "Cannot stream texture: %s", path);
    return result;
}
