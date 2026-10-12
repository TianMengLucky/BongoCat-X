#ifndef BONGO_CAT_STATISTICS_USAGE_PLUGIN_H
#define BONGO_CAT_STATISTICS_USAGE_PLUGIN_H
#include "bongo_cat/statistics_plugin.h"

#define BONGO_CAT_STATISTICS_USAGE_SYMBOL "bongo_cat_statistics_usage_query"
#define BONGO_CAT_STATISTICS_APPS 64u
#define BONGO_CAT_STATISTICS_APP_CAP 256u

/* Worker and application timing ABI v1 companion. Poll callbacks run only on the Rust worker.
   Context remains valid until destroy joins it. No window titles or paths. */
typedef struct BongoCatApplicationSample {
    uint32_t available, active;
    uint64_t idle_ms;
    char id[BONGO_CAT_STATISTICS_APP_CAP], name[BONGO_CAT_STATISTICS_APP_CAP];
} BongoCatApplicationSample;
typedef struct BongoCatApplicationTotal {
    char id[BONGO_CAT_STATISTICS_APP_CAP], name[BONGO_CAT_STATISTICS_APP_CAP];
    uint64_t milliseconds;
} BongoCatApplicationTotal;
typedef struct BongoCatApplicationSnapshot {
    uint32_t struct_size, enabled, available, active, count, export_status;
    uint64_t total_ms, hours[24];
    BongoCatApplicationTotal apps[BONGO_CAT_STATISTICS_APPS];
} BongoCatApplicationSnapshot;
typedef void (*BongoCatStatisticsPoll)(void *context, BongoCatStatisticsMetrics *metrics);
typedef void (*BongoCatApplicationPoll)(void *context, BongoCatApplicationSample *sample);
typedef struct BongoCatStatisticsUsagePlugin {
    uint32_t abi_version, struct_size;
    uint32_t (*configure)(void *instance, void *context,
        BongoCatStatisticsPoll metrics, BongoCatApplicationPoll application);
    uint32_t (*snapshot)(void *instance, uint32_t period, BongoCatApplicationSnapshot *snapshot);
    uint32_t (*set_enabled)(void *instance, uint32_t enabled);
    uint32_t (*export_csv)(void *instance, const char *path);
    uint32_t (*export_status)(void *instance); /* 0 idle, 1 pending, 2 success, 3 failure. */
} BongoCatStatisticsUsagePlugin;
typedef const BongoCatStatisticsUsagePlugin *(*BongoCatStatisticsUsageQuery)(uint32_t abi);
/* Native adapter: worker-only, independently opens any OS resources it uses. */
void bongo_cat_statistics_application_poll(void *context, BongoCatApplicationSample *sample);
#endif
