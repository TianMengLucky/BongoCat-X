#ifndef BONGO_CAT_STATISTICS_KEYBOARD_PLUGIN_H
#define BONGO_CAT_STATISTICS_KEYBOARD_PLUGIN_H
#include <stdint.h>
#define BONGO_CAT_STATISTICS_KEYBOARD_SYMBOL "bongo_cat_statistics_keyboard_query"
#define BONGO_CAT_STATISTICS_KEYS 256u
/* Optional extension. No timestamps, text or ordered key events cross this ABI. */
typedef void (*BongoCatKeyboardPoll)(void *, uint64_t *);
typedef struct BongoCatStatisticsKeyboardPlugin {
    uint32_t abi_version, struct_size;
    uint32_t (*configure)(void *, void *, BongoCatKeyboardPoll);
    uint32_t (*snapshot)(void *, uint32_t, uint64_t *);
} BongoCatStatisticsKeyboardPlugin;
typedef const BongoCatStatisticsKeyboardPlugin *(*BongoCatStatisticsKeyboardQuery)(uint32_t);
#endif
