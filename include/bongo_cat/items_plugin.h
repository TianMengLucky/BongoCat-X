#ifndef BONGO_CAT_ITEMS_PLUGIN_H
#define BONGO_CAT_ITEMS_PLUGIN_H
#include <stdint.h>
#define BONGO_CAT_ITEMS_ABI 1u
#define BONGO_CAT_ITEMS_SYMBOL "bongo_cat_items_query"
#define BONGO_CAT_ITEMS_CAP 16u
/* No allocator, SDL type or graphics handle crosses this ABI. All calls are
   serialized by the host instance state mutex. Strings are bounded UTF-8. Layer 0 is
   in front of the model; negative layers are behind it. */
typedef struct BongoCatItem {
    char asset[80], name[128];
    float x, y, scale, rotation, opacity;
    int32_t layer;
    uint32_t visible, locked, follow_model;
} BongoCatItem;
typedef struct BongoCatItemScene {
    uint32_t struct_size, count;
    BongoCatItem items[BONGO_CAT_ITEMS_CAP];
} BongoCatItemScene;
typedef struct BongoCatItemsPage {
    uint32_t struct_size;
    char id[32], title_key[128], title[64];
} BongoCatItemsPage;
typedef struct BongoCatItemsPlugin {
    uint32_t abi_version, struct_size;
    void *(*create)(void);
    void (*destroy)(void *instance);
    uint32_t (*page)(void *instance, BongoCatItemsPage *page);
    uint32_t (*snapshot)(void *instance, BongoCatItemScene *scene);
    uint32_t (*restore)(void *instance, const BongoCatItemScene *scene);
    uint32_t (*add)(void *instance, const BongoCatItem *item);
    uint32_t (*update)(void *instance, uint32_t index, const BongoCatItem *item);
    uint32_t (*remove)(void *instance, uint32_t index);
} BongoCatItemsPlugin;
typedef const BongoCatItemsPlugin *(*BongoCatItemsQuery)(uint32_t abi);
#endif
