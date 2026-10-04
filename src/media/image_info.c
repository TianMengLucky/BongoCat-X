#include "bongo_cat/file.h"
#include "bongo_cat/image.h"
#include "bongo_cat/safe_ffi.h"
#include "image_internal.h"

#include <stdlib.h>

bool bongo_cat_image_info(const char *path, int *width, int *height) {
    if (width) *width = 0;
    if (height) *height = 0;
    FILE *file = path ? bongo_cat_file_open(path, "rb") : NULL;
    size_t length = 0;
    unsigned char *data = file ? bongo_cat_image_read_stream(file, &length) : NULL;
    if (file) fclose(file);
    int image_width = 0, image_height = 0;
    bool known = data && bongo_safe_image_info(data, length, &image_width,
        &image_height) && image_width > 0 && image_height > 0;
    free(data);
    if (known && width) *width = image_width;
    if (known && height) *height = image_height;
    return known;
}
