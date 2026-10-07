#include "test.h"
#include "rhi_pixels.h"

#include <limits.h>
#include <string.h>

int bongo_cat_test_failures;

int main(void) {
    /* Top-down BGRA with row padding, distinct colors and partial alpha. */
    const uint8_t source[] = {
        1, 2, 3, 4, 5, 6, 7, 8, 99, 99, 99, 99,
        9, 10, 11, 12, 13, 14, 15, 16, 99, 99, 99, 99,
        17, 18, 19, 20, 21, 22, 23, 24, 99, 99, 99, 99};
    uint8_t pixels[24];
    const uint8_t bgra[] = {
        17, 18, 19, 20, 21, 22, 23, 24,
        9, 10, 11, 12, 13, 14, 15, 16,
        1, 2, 3, 4, 5, 6, 7, 8};
    CHECK(bongo_cat_rhi_copy_pixels(source, 2, 3, 12, true,
        0, 0, 2, 3, true, pixels));
    CHECK(memcmp(pixels, bgra, sizeof(bgra)) == 0);
    const uint8_t rgba_rect[] = {23, 22, 21, 24, 15, 14, 13, 16};
    CHECK(bongo_cat_rhi_copy_pixels(source, 2, 3, 12, true,
        1, 0, 1, 2, false, pixels));
    CHECK(memcmp(pixels, rgba_rect, sizeof(rgba_rect)) == 0);
    const uint8_t top[] = {7, 6, 5, 8};
    CHECK(bongo_cat_rhi_copy_pixels(source, 2, 3, 12, true,
        1, 2, 1, 1, false, pixels));
    CHECK(memcmp(pixels, top, sizeof(top)) == 0);
    /* Reverse channel conversion must preserve alpha and premultiplication. */
    const uint8_t rgba[] = {3, 2, 1, 4};
    CHECK(bongo_cat_rhi_copy_pixels(rgba, 1, 1, 4, false,
        0, 0, 1, 1, true, pixels));
    CHECK(memcmp(pixels, source, 4) == 0);
    memset(pixels, 0xaa, sizeof(pixels));
    CHECK(!bongo_cat_rhi_copy_pixels(source, 2, 3, 12, true,
        1, 0, 2, 1, false, pixels));
    CHECK(!bongo_cat_rhi_copy_pixels(source, 2, 3, 12, true,
        0, -1, 1, 1, false, pixels));
    CHECK(!bongo_cat_rhi_copy_pixels(source, 2, 3, 7, true,
        0, 0, 1, 1, false, pixels));
    CHECK(!bongo_cat_rhi_copy_pixels(source, 2, 3, SIZE_MAX, true,
        0, 0, 1, 1, false, pixels));
    CHECK(!bongo_cat_rhi_copy_pixels(source, 2, 3, 12, true,
        INT_MAX, 0, 1, 1, false, pixels));
    for (size_t i = 0; i < sizeof(pixels); ++i) CHECK(pixels[i] == 0xaa);
    return bongo_cat_test_failures ? 1 : 0;
}
