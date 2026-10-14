#include "bongo_cat/items_plugin.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int valid(const BongoCatItem *v) {
    if (!v ||
        !memchr(v->name, 0, sizeof(v->name)) || !v->name[0] ||
        !memchr(v->asset, 0, sizeof(v->asset)) || strlen(v->asset) != 68 ||
        strcmp(v->asset + 64, ".img")) return 0;
    for (unsigned i = 0; i < 64; ++i)
        if (!((v->asset[i] >= '0' && v->asset[i] <= '9') ||
            (v->asset[i] >= 'a' && v->asset[i] <= 'f'))) return 0;
    return isfinite(v->x) && isfinite(v->y) && isfinite(v->scale) &&
        isfinite(v->rotation) && isfinite(v->opacity) &&
        v->x >= 0 && v->x <= 100 && v->y >= 0 && v->y <= 100 &&
        v->scale >= 1 && v->scale <= 200 &&
        v->rotation >= -180 && v->rotation <= 180 &&
        v->opacity >= 0 && v->opacity <= 100 &&
        v->layer >= -30 && v->layer <= 30 &&
        v->visible <= 1 && v->locked <= 1 && v->follow_model <= 1;
}
static void *create(void) {
    BongoCatItemScene *scene = calloc(1, sizeof(*scene));
    if (scene) scene->struct_size = sizeof(*scene);
    return scene;
}
static void destroy(void *instance) { free(instance); }
static uint32_t page(void *instance, BongoCatItemsPage *out) {
    if (!instance || !out || out->struct_size != sizeof(*out)) return 0;
    *out = (BongoCatItemsPage){sizeof(*out), "items", "pages.preference.items.title", "Items"};
    return 1;
}
static uint32_t snapshot(void *instance, BongoCatItemScene *out) {
    if (!instance || !out || out->struct_size != sizeof(*out)) return 0;
    *out = *(BongoCatItemScene *)instance;
    return 1;
}
static uint32_t restore(void *instance, const BongoCatItemScene *scene) {
    if (!instance || !scene || scene->struct_size != sizeof(*scene) ||
        scene->count > BONGO_CAT_ITEMS_CAP) return 0;
    for (uint32_t i = 0; i < scene->count; ++i)
        if (!valid(&scene->items[i])) return 0;
    *(BongoCatItemScene *)instance = *scene;
    return 1;
}
static uint32_t add(void *instance, const BongoCatItem *item) {
    BongoCatItemScene *scene = instance;
    if (!scene || scene->count >= BONGO_CAT_ITEMS_CAP || !valid(item)) return 0;
    scene->items[scene->count++] = *item;
    return 1;
}
static uint32_t update(void *instance, uint32_t index, const BongoCatItem *item) {
    BongoCatItemScene *scene = instance;
    if (!scene || index >= scene->count || !valid(item)) return 0;
    scene->items[index] = *item;
    return 1;
}
static uint32_t remove_item(void *instance, uint32_t index) {
    BongoCatItemScene *scene = instance;
    if (!scene || index >= scene->count) return 0;
    memmove(scene->items + index, scene->items + index + 1,
        (scene->count - index - 1) * sizeof(*scene->items));
    scene->items[--scene->count] = (BongoCatItem){0};
    return 1;
}
#ifdef _WIN32
#define ITEMS_EXPORT __declspec(dllexport)
#else
#define ITEMS_EXPORT __attribute__((visibility("default")))
#endif
ITEMS_EXPORT const BongoCatItemsPlugin *bongo_cat_items_query(uint32_t abi) {
    static const BongoCatItemsPlugin api = {BONGO_CAT_ITEMS_ABI, sizeof(api),
        create, destroy, page, snapshot, restore, add, update, remove_item};
    return abi == BONGO_CAT_ITEMS_ABI ? &api : NULL;
}
