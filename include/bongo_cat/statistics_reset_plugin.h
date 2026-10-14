#ifndef BONGO_CAT_STATISTICS_RESET_PLUGIN_H
#define BONGO_CAT_STATISTICS_RESET_PLUGIN_H
#include <stdint.h>
#define BONGO_CAT_STATISTICS_RESET_ABI 1u
#define BONGO_CAT_STATISTICS_RESET_SYMBOL "bongo_cat_statistics_reset_query"
/* Optional extension; the base statistics ABI remains unchanged.
   reset queues an atomic clear of input and application history.
   status: 0 idle, 1 pending, 2 complete, 3 failed. Settings are preserved. */
typedef struct BongoCatStatisticsResetPlugin {
    uint32_t abi_version, struct_size;
    uint32_t (*reset)(void *instance);
    uint32_t (*status)(void *instance);
} BongoCatStatisticsResetPlugin;
typedef const BongoCatStatisticsResetPlugin *(*BongoCatStatisticsResetQuery)(uint32_t abi);
#endif
