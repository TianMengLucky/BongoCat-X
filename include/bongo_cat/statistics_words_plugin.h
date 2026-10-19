#ifndef BONGO_CAT_STATISTICS_WORDS_PLUGIN_H
#define BONGO_CAT_STATISTICS_WORDS_PLUGIN_H
#include <stdint.h>
#define BONGO_CAT_STATISTICS_WORDS_SYMBOL "bongo_cat_statistics_words_query"
#define BONGO_CAT_STATISTICS_WORDS_TOP 32u
/* Only unordered Han counts cross this optional v1 extension. */
typedef struct BongoCatStatisticsCharacter {
    uint32_t codepoint, reserved;
    uint64_t count;
} BongoCatStatisticsCharacter;
typedef struct BongoCatStatisticsWordsSnapshot {
    uint32_t status, count; /* 0 unsupported, 1 subscribed, 2 failed, 3 events lost. */
    uint64_t total, recent;
    double average; /* Current-session Han/minute, excluding gaps over five seconds. */
    BongoCatStatisticsCharacter top[BONGO_CAT_STATISTICS_WORDS_TOP];
} BongoCatStatisticsWordsSnapshot;
typedef struct BongoCatStatisticsWordsPlugin {
    uint32_t abi_version, struct_size;
    uint32_t (*snapshot)(void *, uint32_t, BongoCatStatisticsWordsSnapshot *);
} BongoCatStatisticsWordsPlugin;
typedef const BongoCatStatisticsWordsPlugin *(*BongoCatStatisticsWordsQuery)(uint32_t);
/* Separate optional v1 timer API preserves the words snapshot ABI. Session-only. */
#define BONGO_CAT_STATISTICS_TIMER_SYMBOL "bongo_cat_statistics_timer_query"
typedef struct BongoCatStatisticsTimerSnapshot {
    uint32_t enabled, reserved;
    uint64_t typing_ms, idle_ms;
} BongoCatStatisticsTimerSnapshot;
typedef struct BongoCatStatisticsTimerPlugin {
    uint32_t abi_version, struct_size;
    uint32_t (*snapshot)(void *, BongoCatStatisticsTimerSnapshot *);
    uint32_t (*enable)(void *, uint32_t);
} BongoCatStatisticsTimerPlugin;
typedef const BongoCatStatisticsTimerPlugin *(*BongoCatStatisticsTimerQuery)(uint32_t);
/* Optional setting extension; existing words and timer layouts stay unchanged. */
#define BONGO_CAT_STATISTICS_BACKSPACE_SYMBOL "bongo_cat_statistics_backspace_query"
typedef struct BongoCatStatisticsBackspacePlugin {
    uint32_t abi_version, struct_size;
    uint32_t (*snapshot)(void *, uint32_t *);
    uint32_t (*enable)(void *, uint32_t);
} BongoCatStatisticsBackspacePlugin;
typedef const BongoCatStatisticsBackspacePlugin *(*BongoCatStatisticsBackspaceQuery)(uint32_t);
#endif
