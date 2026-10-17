#include "bongo_cat/image.h"
#include "bongo_cat/safe_ffi.h"

bool bongo_cat_image_info(const char *path, int *width, int *height) {
    if (width) *width = 0;
    if (height) *height = 0;
    int image_width = 0, image_height = 0;
    bool known = path && bongo_safe_image_info_file(path, &image_width,
        &image_height) && image_width > 0 && image_height > 0;
    if (known && width) *width = image_width;
    if (known && height) *height = image_height;
    return known;
}
