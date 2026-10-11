#ifndef BONGO_CAT_STATISTICS_PAGE_PLUGIN_H
#define BONGO_CAT_STATISTICS_PAGE_PLUGIN_H

#include <stdint.h>

#define BONGO_CAT_STATISTICS_PAGE_ABI 1u
#define BONGO_CAT_STATISTICS_PAGE_SYMBOL "bongo_cat_statistics_page_query"
#define BONGO_CAT_STATISTICS_PAGE_ICON_ACTIVITY 1u

/* A plugin contributes navigation only while its instance is active.
   The host copies this descriptor before registration and removes it before
   background retirement. All strings are bounded, NUL-terminated UTF-8. */
typedef struct BongoCatStatisticsPage {
    uint32_t struct_size;
    char id[32], title_key[128], title[64];
    uint32_t icon;
} BongoCatStatisticsPage;

typedef struct BongoCatStatisticsPagePlugin {
    uint32_t abi_version, struct_size;
    uint32_t (*page)(void *instance, BongoCatStatisticsPage *page);
} BongoCatStatisticsPagePlugin;

typedef const BongoCatStatisticsPagePlugin *(*BongoCatStatisticsPageQuery)(uint32_t abi);

#endif
