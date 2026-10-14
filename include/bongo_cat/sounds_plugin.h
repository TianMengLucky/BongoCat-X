#ifndef BONGO_CAT_SOUNDS_PLUGIN_H
#define BONGO_CAT_SOUNDS_PLUGIN_H
#include <stdint.h>
#define BONGO_CAT_SOUNDS_ABI 1u
#define BONGO_CAT_SOUNDS_SYMBOL "bongo_cat_sounds_query"
#define BONGO_CAT_SOUNDS_CAP 128u
/* Canonical input names match the host platform backends. Positions use key
   units, with six keyboard rows. Calls are serialized on the host thread. */
typedef struct BongoCatSoundKey {
    char id[32], label[16];
    float x, width;
    uint32_t row;
} BongoCatSoundKey;
typedef struct BongoCatSoundBinding { char asset[80], name[128]; } BongoCatSoundBinding;
typedef struct BongoCatSoundScene {
    uint32_t struct_size, count;
    BongoCatSoundBinding bindings[BONGO_CAT_SOUNDS_CAP];
} BongoCatSoundScene;
typedef struct BongoCatSoundsPage { uint32_t struct_size; char id[32], title_key[128], title[64]; } BongoCatSoundsPage;
typedef struct BongoCatSoundsPlugin {
    uint32_t abi_version, struct_size;
    void *(*create)(void);
    void (*destroy)(void *instance);
    uint32_t (*page)(void *instance, BongoCatSoundsPage *page);
    uint32_t (*key_count)(void);
    uint32_t (*key)(uint32_t index, BongoCatSoundKey *key);
    uint32_t (*snapshot)(void *instance, BongoCatSoundScene *scene);
    uint32_t (*restore)(void *instance, const BongoCatSoundScene *scene);
    uint32_t (*bind)(void *instance, uint32_t index, const BongoCatSoundBinding *binding);
    /* Tracks releases even while playback is suppressed. Returns a binding
       only for a fresh down transition; OS autorepeat never retriggers. */
    uint32_t (*press)(void *instance, const char *key, uint32_t down, BongoCatSoundBinding *binding);
} BongoCatSoundsPlugin;
typedef const BongoCatSoundsPlugin *(*BongoCatSoundsQuery)(uint32_t abi);
#endif
