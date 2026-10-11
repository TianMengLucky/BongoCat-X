#ifndef BONGO_CAT_STATISTICS_PLUGIN_H
#define BONGO_CAT_STATISTICS_PLUGIN_H

#include <stdint.h>

#define BONGO_CAT_STATISTICS_ABI 1u
#define BONGO_CAT_STATISTICS_SYMBOL "bongo_cat_statistics_plugin_query"
#define BONGO_CAT_STATISTICS_DAYS 30u
#define BONGO_CAT_STATISTICS_BUTTONS 5u

/* Only aggregates cross this ABI: no text, key identities or coordinates.
   Movement and scrolling use backend input units (pixels/points/raw or wheel/line units). */
typedef struct BongoCatStatisticsMetrics {
    uint64_t keys;
    uint64_t buttons[BONGO_CAT_STATISTICS_BUTTONS]; /* Left/right/middle/back/forward. */
    double movement, scroll;
} BongoCatStatisticsMetrics;

typedef struct BongoCatStatisticsDay {
    uint32_t date; /* Local calendar date, YYYYMMDD. */
    BongoCatStatisticsMetrics metrics;
} BongoCatStatisticsDay;

typedef struct BongoCatStatisticsSnapshot {
    uint32_t struct_size, storage_error;
    BongoCatStatisticsMetrics today, week, month, lifetime;
    BongoCatStatisticsMetrics hours[24];
    BongoCatStatisticsDay days[BONGO_CAT_STATISTICS_DAYS]; /* Oldest first; includes today. */
} BongoCatStatisticsSnapshot;

/* snapshot reads a published cache without waiting; ingest/export enqueue.
   destroy/flush are lifecycle barriers and must run away from the pet loop
   during interactive changes. Never unload before destroy joins all workers.
   Export returns queue acceptance; the usage extension reports completion.
   Strings are borrowed UTF-8. */
typedef struct BongoCatStatisticsPlugin {
    uint32_t abi_version, struct_size;
    void *(*create)(const char *directory);
    uint32_t (*destroy)(void *instance);
    void (*ingest)(void *instance, const BongoCatStatisticsMetrics *metrics);
    uint32_t (*snapshot)(void *instance, BongoCatStatisticsSnapshot *snapshot);
    uint32_t (*flush)(void *instance);
    uint32_t (*export_csv)(void *instance, const char *path);
} BongoCatStatisticsPlugin;

typedef const BongoCatStatisticsPlugin *(*BongoCatStatisticsQuery)(uint32_t abi);

#endif
