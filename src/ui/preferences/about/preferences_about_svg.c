#include "preferences_about_svg.h"
#include <math.h>
#include <stdlib.h>
#define NANOSVG_IMPLEMENTATION
#include "../../../../vendor/nanosvg/nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "../../../../vendor/nanosvg/nanosvgrast.h"

static unsigned char *svg_pixels(char *svg, int size) {
    NSVGimage *image = nsvgParse(svg, "px", 96);
    if (!image)
        return NULL;
    unsigned char *pixels = NULL;
    if (isfinite(image->width) && isfinite(image->height) && image->width > 0 &&
        image->height > 0 && image->width <= 4096 && image->height <= 4096) {
        NSVGrasterizer *raster = nsvgCreateRasterizer();
        pixels = calloc((size_t)size * (size_t)size, 4);
        if (raster && pixels) {
            float scale = (float)size / fmaxf(image->width, image->height);
            nsvgRasterize(raster, image, ((float)size - image->width * scale) * .5f,
                          ((float)size - image->height * scale) * .5f, scale, pixels, size, size, size * 4);
        } else {
            free(pixels);
            pixels = NULL;
        }
        nsvgDeleteRasterizer(raster);
    }
    nsvgDelete(image);
    return pixels;
}

int bongo_cat_about_svg_parse_xml(char *input,
    void (*start)(void *, const char *, const char **),
    void (*end)(void *, const char *), void (*content)(void *, const char *), void *user) {
    return nsvg__parseXML(input, start, end, content, user);
}
