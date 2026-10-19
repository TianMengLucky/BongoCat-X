#ifndef BONGO_CAT_LAN_PLUGIN_H
#define BONGO_CAT_LAN_PLUGIN_H
#include "bongo_cat/plugin_ui_page.h"
#define BONGO_CAT_LAN_SYMBOL "bongo_cat_lan_query_v2"
#define BONGO_CAT_LAN_PEERS 7u
#define BONGO_CAT_LAN_PARAMETERS 16u
/* Main thread only; plugin copies descriptors. Paths never go onto the wire.
   destroy cancels/joins networking. tick freezes the roster; peer reads that roster.
   Tear down absent peers before release acknowledges the frozen roster epoch. */
typedef struct BongoCatLanModel {
    char directory[4096], adapter[4096], setting[4096], digest[65];
} BongoCatLanModel;
typedef struct BongoCatLanPose {
    uint32_t mask;
    float values[BONGO_CAT_LAN_PARAMETERS];
    float pointer_x, pointer_y;
    uint32_t buttons;
} BongoCatLanPose;
typedef struct BongoCatLanSnapshot {
    uint32_t struct_size, connected, generation, status, peer_id, peer_count;
    BongoCatLanModel model;
    BongoCatLanPose pose;
} BongoCatLanSnapshot;
typedef struct BongoCatLanPlugin {
    uint32_t abi_version, struct_size;
    void *(*create)(const char *);
    void (*destroy)(void *);
    void (*disconnect)(void *);
    uint32_t (*models)(void *, const BongoCatLanModel *, uint32_t, uint32_t);
    void (*tick)(void *, const BongoCatLanPose *, BongoCatLanSnapshot *);
    uint32_t (*peer)(void *, uint32_t, BongoCatLanSnapshot *);
    void (*release)(void *);
    const char *(*friend_name)(void *, uint32_t);
    uint32_t (*settings)(void *, BongoCatPluginUi *, const BongoCatPluginPageUiHost *,
        const BongoCatPluginPageUiServices *);
} BongoCatLanPlugin;
typedef const BongoCatLanPlugin *(*BongoCatLanQuery)(uint32_t);
#endif
