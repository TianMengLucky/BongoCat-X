#include "items_internal.h"
#include "test.h"
#include <stdlib.h>
#include <string.h>

int bongo_cat_test_failures;
static bool snapshot_available = true;

static uint32_t snapshot(void *instance, BongoCatItemScene *scene) {
    if (!snapshot_available) return 0;
    *scene = *(BongoCatItemScene *)instance;
    return 1;
}

static void cache_image(BongoCatItems *items, unsigned index, const char *asset) {
    SDL_strlcpy(items->images[index].asset, asset, sizeof(items->images[index].asset));
    BongoCatImage *image = &items->images[index].image;
    image->width = image->height = 1;
    image->pixels = calloc(1, sizeof(uint32_t));
    CHECK(image->pixels != NULL);
    image->surface = SDL_CreateSurfaceFrom(1, 1, SDL_PIXELFORMAT_RGBA32,
        image->pixels, sizeof(uint32_t));
    CHECK(image->surface != NULL);
}

int main(void) {
    BongoCatApp *app = calloc(1, sizeof(*app));
    BongoCatItems *items = calloc(1, sizeof(*items));
    CHECK(app && items);
    if (!app || !items) { free(items); free(app); return 1; }
    BongoCatItemScene scene = {0};
    scene.struct_size = sizeof(scene);
    const BongoCatItemsPlugin api = {.snapshot = snapshot};
    items->api = &api; items->instance = &scene;
    atomic_init(&items->loaded, true);
    app->items = items;
    cache_image(items, 0, "shared.img");
    cache_image(items, 1, "hidden.img");
    cache_image(items, 2, "transparent.img");
    cache_image(items, 3, "removed.img");
    scene.count = 4;
    SDL_strlcpy(scene.items[0].asset, "shared.img", sizeof(scene.items[0].asset));
    scene.items[0].opacity = 100;
    scene.items[1] = scene.items[0]; scene.items[1].visible = 1;
    SDL_strlcpy(scene.items[2].asset, "hidden.img", sizeof(scene.items[2].asset));
    scene.items[2].opacity = 100;
    SDL_strlcpy(scene.items[3].asset, "transparent.img", sizeof(scene.items[3].asset));
    scene.items[3].visible = 1;
    unsigned char *shared = items->images[0].image.pixels;
    SDL_Surface *surface = items->images[0].image.surface;
    bongo_cat_items_prune_graphics(app);
    CHECK(items->images[0].image.pixels == shared);
    CHECK(items->images[0].image.surface == surface);
    for (unsigned i = 1; i < scene.count; ++i) {
        CHECK(!items->images[i].asset[0]);
        CHECK(items->images[i].image.pixels == NULL);
        CHECK(items->images[i].image.surface == NULL);
    }
    /* Repeated edits retain the same allocation while any duplicate is visible. */
    bongo_cat_items_prune_graphics(app);
    scene.items[0].visible = 1; scene.items[1].visible = 0;
    bongo_cat_items_prune_graphics(app);
    CHECK(items->images[0].image.pixels == shared);
    /* Failed snapshots do not discard resources for an unknown scene. */
    snapshot_available = false; scene.count = 0;
    bongo_cat_items_prune_graphics(app);
    CHECK(items->images[0].image.pixels == shared);
    snapshot_available = true;
    bongo_cat_items_prune_graphics(app);
    CHECK(items->images[0].image.pixels == NULL && !items->images[0].asset[0]);
    /* An invalid GL context must preserve handles for a later retry. */
    items->images[0].texture = 1;
    SDL_strlcpy(items->images[0].asset, "gpu.img", sizeof(items->images[0].asset));
    bongo_cat_items_prune_graphics(app);
    CHECK(items->images[0].texture == 1 && items->images[0].asset[0]);
    items->images[0].texture = 0;
    cache_image(items, 0, "reloaded.img");
    bongo_cat_items_release_graphics(app);
    CHECK(items->images[0].image.pixels == NULL && !items->images[0].asset[0]);
    free(items); free(app);
    return bongo_cat_test_failures ? 1 : 0;
}
