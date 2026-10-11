#include "bongo_cat/image.h"
#include "test.h"

#include <math.h>
#include <string.h>

int bongo_cat_test_failures;

int main(void) {
    unsigned char pixels[4 * 4 * 4] = {0};
    BongoCatImage image = {.pixels = pixels, .width = 4, .height = 4};
    float bounds[4] = {-1, -1, -1, -1};
    /* RGB in transparent pixels and alpha <= 8 must not enlarge the bounds. */
    pixels[0] = 255; pixels[3] = 8;
    pixels[(1 * 4 + 2) * 4 + 3] = 9;
    pixels[(2 * 4 + 1) * 4 + 3] = 255;
    CHECK(bongo_cat_image_opaque_bounds_pixels(&image,
        &bounds[0], &bounds[1], &bounds[2], &bounds[3]));
    CHECK(fabsf(bounds[0] - 0.25f) < 0.0001f);
    CHECK(fabsf(bounds[1] - 0.25f) < 0.0001f);
    CHECK(fabsf(bounds[2] - 0.75f) < 0.0001f);
    CHECK(fabsf(bounds[3] - 0.75f) < 0.0001f);
    memset(pixels, 0, sizeof(pixels));
    CHECK(!bongo_cat_image_opaque_bounds_pixels(&image,
        &bounds[0], &bounds[1], &bounds[2], &bounds[3]));
    CHECK(fabsf(bounds[0] - 0.25f) < 0.0001f);
    CHECK(!bongo_cat_image_opaque_bounds_pixels(NULL,
        &bounds[0], &bounds[1], &bounds[2], &bounds[3]));
    image.width = 0;
    CHECK(!bongo_cat_image_opaque_bounds_pixels(&image,
        &bounds[0], &bounds[1], &bounds[2], &bounds[3]));
    image.width = 4;
    CHECK(!bongo_cat_image_opaque_bounds_pixels(&image,
        NULL, &bounds[1], &bounds[2], &bounds[3]));
    pixels[3] = pixels[sizeof(pixels) - 1] = 255;
    CHECK(bongo_cat_image_opaque_bounds_pixels(&image,
        &bounds[0], &bounds[1], &bounds[2], &bounds[3]));
    CHECK(fabsf(bounds[0]) < 0.0001f && fabsf(bounds[1]) < 0.0001f);
    CHECK(fabsf(bounds[2] - 1.0f) < 0.0001f && fabsf(bounds[3] - 1.0f) < 0.0001f);
    return bongo_cat_test_failures ? 1 : 0;
}
