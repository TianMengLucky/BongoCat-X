#include "rhi_internal.h"
#include "rhi_corners.h"
#include "test.h"

#include <limits.h>
#include <string.h>

int bongo_cat_test_failures;

static void fill_frame(unsigned char pixels[72]) {
    memset(pixels, 0xaa, 72);
    const unsigned char color[] = {20, 40, 60, 80};
    for (int y = 0; y < 3; ++y) for (int x = 0; x < 4; ++x)
        memcpy(pixels + y * 24 + x * 4, color, sizeof(color));
}

static void check_channels(BongoCatRhi *rhi, bool bgra, bool cropped) {
    unsigned char pixels[72], expected[72];
    fill_frame(pixels); memcpy(expected, pixels, sizeof(pixels));
    const unsigned char partial_rgba[] = {42, 36, 38, 168};
    const unsigned char partial_bgra[] = {18, 36, 62, 168};
    const unsigned char opaque_rgba[] = {11, 22, 33, 255};
    const unsigned char opaque_bgra[] = {33, 22, 11, 255};
    memcpy(expected + 24 + 4, bgra ? partial_bgra : partial_rgba, 4);
    memcpy(expected + 24 + 8, bgra ? opaque_bgra : opaque_rgba, 4);
    SDL_Rect changed;
    CHECK(bongo_cat_rhi_compose_foreground(rhi, pixels, 4, 3, 24, bgra, &changed));
    if (cropped)
        CHECK(changed.x == 1 && changed.y == 1 && changed.w == 2 && changed.h == 1);
    else
        CHECK(changed.x == 0 && changed.y == 0 && changed.w == 4 && changed.h == 3);
    /* Untouched pixels and destination padding must remain byte-identical. */
    CHECK(memcmp(pixels, expected, sizeof(pixels)) == 0);
}

static void check_cache(BongoCatRhi *rhi) {
    unsigned char source[60] = {0}; /* 4x3 RGBA with padded rows. */
    const unsigned char partial[] = {32, 16, 8, 128};
    const unsigned char opaque[] = {11, 22, 33, 255};
    memcpy(source + 20 + 4, partial, 4);
    memcpy(source + 20 + 8, opaque, 4);
    source[0] = 255; /* Nonzero RGB at alpha zero stays invisible. */
    CHECK(bongo_cat_rhi_set_foreground(rhi, source, 4, 3, 20, 1));
    check_channels(rhi, false, true); check_channels(rhi, true, true);
    unsigned char *allocation = rhi->foreground_pixels;
    memset(source, 0, sizeof(source));
    CHECK(bongo_cat_rhi_set_foreground(rhi, source, 4, 3, 20, 1));
    check_channels(rhi, false, true); /* Same revision preserves the cached layer. */
    /* If the window cache is unavailable/corrupt, composition stays complete. */
    SDL_SetNumberProperty(SDL_GetWindowProperties(rhi->window),
        "BongoCat.ForegroundWidth", INT_MAX);
    check_channels(rhi, false, false);
    /* Moving a sprite must replace its bounds, including the last padded row. */
    memcpy(source + 40 + 12, opaque, sizeof(opaque));
    CHECK(bongo_cat_rhi_set_foreground(rhi, source, 4, 3, 20, 2));
    CHECK(rhi->foreground_pixels == allocation);
    unsigned char pixels[72], expected[72];
    fill_frame(pixels); memcpy(expected, pixels, sizeof(pixels));
    memcpy(expected + 48 + 12, opaque, sizeof(opaque));
    SDL_Rect changed = {1, 1, 1, 1};
    CHECK(bongo_cat_rhi_compose_foreground(rhi, pixels, 4, 3, 24, false, &changed));
    CHECK(changed.x == 3 && changed.y == 2 && changed.w == 1 && changed.h == 1);
    CHECK(memcmp(pixels, expected, sizeof(pixels)) == 0);
    memset(source, 0, sizeof(source));
    CHECK(bongo_cat_rhi_set_foreground(rhi, source, 4, 3, 20, 3));
    fill_frame(pixels); memcpy(expected, pixels, sizeof(pixels));
    CHECK(!bongo_cat_rhi_compose_foreground(rhi, pixels, 4, 3, 24, false, &changed));
    CHECK(changed.w == 0 && changed.h == 0);
    CHECK(memcmp(pixels, expected, sizeof(pixels)) == 0);
    CHECK(!bongo_cat_rhi_compose_foreground(rhi, pixels, 4, 3, 15, false, &changed));
    CHECK(!bongo_cat_rhi_compose_foreground(rhi, pixels, 4, 3, SIZE_MAX, false, &changed));
    CHECK(!bongo_cat_rhi_set_foreground(rhi, source, 4, 3, 15, 4));
    CHECK(!bongo_cat_rhi_set_foreground(rhi, source, INT_MAX, 3, 20, 4));
    unsigned char one[] = {3, 2, 1, 255}, target[4] = {0};
    CHECK(bongo_cat_rhi_set_foreground(rhi, one, 1, 1, 4, 4));
    CHECK(bongo_cat_rhi_compose_foreground(rhi, target, 1, 1, 4, false, &changed));
    CHECK(changed.x == 0 && changed.y == 0 && changed.w == 1 && changed.h == 1);
    CHECK(memcmp(one, target, sizeof(one)) == 0);
}

static void check_corners(BongoCatRhi *rhi) {
    unsigned char source[48] = {0}, target[48] = {0};
    const unsigned char red[] = {255, 0, 0, 255};
    memcpy(source, red, sizeof(red));
    memcpy(source + (1 * 4 + 1) * 4, red, sizeof(red));
    bongo_cat_rhi_store_corners(rhi->window, (BongoCatCornerRect){0, 0, 4, 3, 1000});
    CHECK(bongo_cat_rhi_set_foreground(rhi, source, 4, 3, 16, 5));
    SDL_Rect changed;
    CHECK(bongo_cat_rhi_compose_foreground(rhi, target, 4, 3, 16, false, &changed));
    CHECK(changed.x == 0 && changed.y == 0 && changed.w == 2 && changed.h == 2);
    CHECK(target[0] == 202 && target[3] == 202);
    CHECK(memcmp(target + (1 * 4 + 1) * 4, red, sizeof(red)) == 0);
    CHECK(bongo_cat_rhi_set_foreground(rhi, NULL, 0, 0, 0, 0));
    CHECK(!rhi->foreground_pixels && !rhi->foreground_width && !rhi->foreground_height);
    CHECK(!bongo_cat_rhi_compose_foreground(rhi, target, 4, 3, 16, false, &changed));
}

int main(void) {
    /* Only a dummy window for corner properties; no graphics device is used. */
    CHECK(SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy"));
    CHECK(SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *window = SDL_CreateWindow("foreground-test", 4, 3, SDL_WINDOW_HIDDEN);
    CHECK(window != NULL);
    if (window) {
        BongoCatRhi rhi = {.window = window};
        check_cache(&rhi); check_corners(&rhi);
        bongo_cat_rhi_set_foreground(&rhi, NULL, 0, 0, 0, 0);
        SDL_DestroyWindow(window);
    }
    SDL_Quit();
    return bongo_cat_test_failures ? 1 : 0;
}
