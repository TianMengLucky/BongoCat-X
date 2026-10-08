#include "bongo_cat/i18n.h"
#include "bongo_cat/file.h"
#include "bongo_cat/path.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "bongo_cat/json_dom.h"

/* Lookup accelerator: every user-facing string is resolved through this API,
   and the settings window re-resolves all of its labels on every rendered
   frame. Resolving a dotted key re-split the path and walked the Rust JSON
   object (a linear scan over its members) on every call, so the same few
   hundred keys were re-walked hundreds of times per frame. Caching the
   resolved pointer turns the steady state into one hash probe. The cache is
   owned by the main thread only, like every caller of this API, and is
   dropped whenever the active document changes. */
#define I18N_CACHE_CAP 512
#define I18N_KEY_CAP 64

typedef struct I18nCacheEntry {
    char key[I18N_KEY_CAP];
    const char *value;
} I18nCacheEntry;

struct BongoCatI18n {
    char root[BONGO_CAT_PATH_CAP];
    BongoCatLanguage language;
    BongoJsonDoc *fallback;
    BongoJsonDoc *active;
    I18nCacheEntry *cache;
    size_t cache_used;
};

static BongoJsonDoc *load_locale(const char *root, BongoCatLanguage language,
    BongoCatError *error) {
    char name[32], path[BONGO_CAT_PATH_CAP];
    snprintf(name, sizeof(name), "%s.json", bongo_cat_language_name(language));
    if (!bongo_cat_path_join(path, sizeof(path), root, name)) return NULL;
    BongoJsonReadError json_error = {0};
    FILE *file = bongo_cat_file_open(path, "rb");
    BongoJsonDoc *doc = file ? bongo_json_read_fp(file, 0, NULL, &json_error) : NULL;
    if (file) fclose(file);
    if (!doc) bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
        "Cannot load locale %s: %s", path,
        json_error.msg[0] ? json_error.msg : "cannot open file");
    return doc;
}

BongoCatI18n *bongo_cat_i18n_create(const char *root, BongoCatLanguage language,
    BongoCatError *error) {
    if (!root) return NULL;
    BongoCatI18n *value = calloc(1, sizeof(*value));
    if (!value) return NULL;
    /* A failed cache allocation only costs the accelerator, not correctness. */
    value->cache = calloc(I18N_CACHE_CAP, sizeof(*value->cache));
    snprintf(value->root, sizeof(value->root), "%s", root);
    value->fallback = load_locale(root, BONGO_CAT_LANG_EN_US, error);
    if (!value->fallback) {
        free(value->cache);
        free(value);
        return NULL;
    }
    if (bongo_cat_i18n_reload(value, language, error) != BONGO_CAT_OK) {
        bongo_cat_i18n_destroy(value);
        return NULL;
    }
    return value;
}

void bongo_cat_i18n_destroy(BongoCatI18n *value) {
    if (!value) return;
    if (value->active && value->active != value->fallback) bongo_json_doc_free(value->active);
    bongo_json_doc_free(value->fallback);
    free(value->cache);
    free(value);
}

BongoCatResult bongo_cat_i18n_reload(BongoCatI18n *value, BongoCatLanguage language,
    BongoCatError *error) {
    if (!value) return BONGO_CAT_ERROR_ARGUMENT;
    BongoJsonDoc *next = language == BONGO_CAT_LANG_EN_US
        ? value->fallback : load_locale(value->root, language, error);
    if (!next) return BONGO_CAT_ERROR_FORMAT;
    if (value->active && value->active != value->fallback) bongo_json_doc_free(value->active);
    value->active = next;
    value->language = language;
    /* Cached pointers into the previous document are now dangling. */
    if (value->cache) {
        memset(value->cache, 0, I18N_CACHE_CAP * sizeof(*value->cache));
        value->cache_used = 0;
    }
    return BONGO_CAT_OK;
}

static BongoJsonValue *find_value(BongoJsonDoc *doc, const char *key) {
    BongoJsonValue *value = doc ? bongo_json_doc_get_root(doc) : NULL;
    const char *cursor = key;
    while (value && cursor && *cursor) {
        const char *dot = strchr(cursor, '.');
        size_t length = dot ? (size_t)(dot - cursor) : strlen(cursor);
        char part[64];
        if (!length || length >= sizeof(part)) return NULL;
        memcpy(part, cursor, length);
        part[length] = '\0';
        value = bongo_json_is_obj(value) ? bongo_json_obj_get(value, part) : NULL;
        cursor = dot ? dot + 1 : NULL;
    }
    return value;
}

/* Resolves `key` against the active document, then the English fallback.
   Returns NULL when neither document defines it. */
static const char *resolve(const BongoCatI18n *value, const char *key) {
    BongoJsonValue *found = find_value(value->active, key);
    if (!bongo_json_is_str(found)) found = find_value(value->fallback, key);
    return bongo_json_is_str(found) ? bongo_json_get_str(found) : NULL;
}

static size_t key_hash(const char *key, size_t length) {
    uint64_t hash = 1469598103934665603ull;
    for (size_t index = 0; index < length; ++index) {
        hash ^= (unsigned char)key[index];
        hash *= 1099511628211ull;
    }
    return (size_t)hash;
}

/* The resolved string is owned by the document, so a cached pointer stays
   valid until `bongo_cat_i18n_reload` empties the table. The cast only
   updates this accelerator; the public signature stays const. */
const char *bongo_cat_i18n_get(const BongoCatI18n *value, const char *key,
    const char *fallback) {
    if (!value || !key) return fallback;
    BongoCatI18n *owner = (BongoCatI18n *)value;
    size_t length = strlen(key);
    if (!owner->cache || !length || length >= I18N_KEY_CAP) {
        const char *found = resolve(value, key);
        return found ? found : fallback;
    }
    size_t slot = key_hash(key, length) & (I18N_CACHE_CAP - 1);
    for (size_t probe = 0; probe < I18N_CACHE_CAP; ++probe) {
        const I18nCacheEntry *entry = &owner->cache[slot];
        if (!entry->key[0]) break;
        if (!strcmp(entry->key, key))
            return entry->value ? entry->value : fallback;
        slot = (slot + 1) & (I18N_CACHE_CAP - 1);
    }
    const char *found = resolve(value, key);
    /* Stop inserting once the table is mostly full so probes stay short. */
    if (owner->cache_used < I18N_CACHE_CAP - I18N_CACHE_CAP / 4) {
        slot = key_hash(key, length) & (I18N_CACHE_CAP - 1);
        while (owner->cache[slot].key[0] &&
            strcmp(owner->cache[slot].key, key))
            slot = (slot + 1) & (I18N_CACHE_CAP - 1);
        if (!owner->cache[slot].key[0]) {
            memcpy(owner->cache[slot].key, key, length + 1);
            owner->cache[slot].value = found;
            owner->cache_used++;
        }
    }
    return found ? found : fallback;
}

static uint32_t decode_utf8(const unsigned char **cursor) {
    const unsigned char *s = *cursor;
    uint32_t value;
    if (*s < 0x80) { *cursor = s + 1; return *s; }
    if ((*s & 0xe0) == 0xc0) {
        value = (uint32_t)(*s & 0x1f); s++;
        if ((*s & 0xc0) != 0x80) return 0;
        value = (value << 6) | (*s++ & 0x3f);
    } else if ((*s & 0xf0) == 0xe0) {
        value = (uint32_t)(*s & 0x0f); s++;
        for (int i = 0; i < 2; ++i) {
            if ((*s & 0xc0) != 0x80) return 0;
            value = (value << 6) | (*s++ & 0x3f);
        }
    } else if ((*s & 0xf8) == 0xf0) {
        value = (uint32_t)(*s & 0x07); s++;
        for (int i = 0; i < 3; ++i) {
            if ((*s & 0xc0) != 0x80) return 0;
            value = (value << 6) | (*s++ & 0x3f);
        }
    } else { *cursor = s + 1; return 0; }
    *cursor = s;
    return value;
}

static void add_point(uint32_t *points, size_t *count, uint32_t point) {
    if (point <= 0x7e || point > 0x10ffff || *count >= 4096) return;
    for (size_t i = 0; i < *count; ++i) if (points[i] == point) return;
    points[(*count)++] = point;
}

static void collect_value(BongoJsonValue *value, uint32_t *points, size_t *count) {
    if (bongo_json_is_str(value)) {
        const unsigned char *cursor = (const unsigned char *)bongo_json_get_str(value);
        while (*cursor) add_point(points, count, decode_utf8(&cursor));
    } else if (bongo_json_is_arr(value)) {
        size_t index, maximum; BongoJsonValue *item;
        bongo_json_arr_foreach(value, index, maximum, item) collect_value(item, points, count);
    } else if (bongo_json_is_obj(value)) {
        size_t index, maximum; BongoJsonValue *key, *item;
        bongo_json_obj_foreach(value, index, maximum, key, item) {
            (void)key;
            collect_value(item, points, count);
        }
    }
}

static int compare_point(const void *left, const void *right) {
    uint32_t a = *(const uint32_t *)left, b = *(const uint32_t *)right;
    return a < b ? -1 : a > b;
}

static size_t build_ranges(uint32_t *points, size_t count,
    uint32_t *ranges, size_t capacity) {
    size_t written = 2;
    const unsigned char *builtins =
        (const unsigned char *)"English Fran\xC3\xA7" "ais Deutsch "
        "Portugu\xC3\xAAs Espa\xC3\xB1" "ol"
        "\xE7\xAE\x80\xE4\xBD\x93\xE4\xB8\xAD\xE6\x96\x87"
        "\xE7\xB9\x81\xE9\xAB\x94\xE4\xB8\xAD\xE6\x96\x87"
        "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"
        "\xED\x95\x9C\xEA\xB5\xAD\xEC\x96\xB4"
        "\xD0\xA0\xD1\x83\xD1\x81\xD1\x81\xD0\xBA\xD0\xB8\xD0\xB9";
    while (*builtins) add_point(points, &count, decode_utf8(&builtins));
    qsort(points, count, sizeof(points[0]), compare_point);
    ranges[0] = 0x20; ranges[1] = 0x7e;
    size_t i = 0;
    while (i < count && points[i] <= 0x7e) i++;
    while (i < count && written + 2 < capacity) {
        uint32_t first = points[i], last = first;
        while (++i < count && points[i] <= last + 1)
            if (points[i] > last) last = points[i];
        ranges[written++] = first;
        ranges[written++] = last;
    }
    ranges[written] = 0;
    return written + 1;
}

size_t bongo_cat_i18n_glyph_ranges(const BongoCatI18n *value, uint32_t *ranges,
    size_t capacity) {
    if (!value || !ranges || capacity < 3) return 0;
    uint32_t points[4096]; size_t count = 0;
    collect_value(bongo_json_doc_get_root(value->active), points, &count);
    return build_ranges(points, count, ranges, capacity);
}

const char *bongo_cat_ui_language_name(BongoCatLanguage language) {
    switch (language) {
    case BONGO_CAT_LANG_EN_US: return "English";
    case BONGO_CAT_LANG_ZH_CN: return "简体中文";
    case BONGO_CAT_LANG_ZH_HANT: return "繁體中文";
    case BONGO_CAT_LANG_FR_FR: return "Français";
    case BONGO_CAT_LANG_DE_DE: return "Deutsch";
    case BONGO_CAT_LANG_JA_JP: return "日本語";
    case BONGO_CAT_LANG_KO_KR: return "한국어";
    case BONGO_CAT_LANG_PT_BR: return "Português";
    case BONGO_CAT_LANG_RU_RU: return "Русский";
    case BONGO_CAT_LANG_ES_ES: return "Español";
    default: return "";
    }
}

size_t bongo_cat_i18n_all_glyph_ranges(const BongoCatI18n *value, uint32_t *ranges,
    size_t capacity) {
    if (!value || !ranges || capacity < 3) return 0;
    /* The settings window renders the native language names verbatim
       regardless of the active language, so their scripts are collected
       first and can never be crowded out by the locale strings. */
    uint32_t points[8192]; size_t count = 0;
    for (int language = 0; language < BONGO_CAT_LANG_COUNT; ++language) {
        const unsigned char *cursor = (const unsigned char *)
            bongo_cat_ui_language_name((BongoCatLanguage)language);
        while (*cursor) add_point(points, &count, decode_utf8(&cursor));
    }
    collect_value(bongo_json_doc_get_root(value->fallback), points, &count);
    for (int language = 0; language < BONGO_CAT_LANG_COUNT; ++language) {
        if (language == BONGO_CAT_LANG_EN_US) continue;
        BongoCatError ignored = {0};
        BongoJsonDoc *doc = load_locale(value->root, (BongoCatLanguage)language,
            &ignored);
        if (doc) { collect_value(bongo_json_doc_get_root(doc), points, &count);
            bongo_json_doc_free(doc); }
    }
    return build_ranges(points, count, ranges, capacity);
}
