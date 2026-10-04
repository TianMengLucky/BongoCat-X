#include "bongo_cat/i18n.h"
#include "bongo_cat/file.h"
#include "bongo_cat/path.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <yyjson.h>

struct BongoCatI18n {
    char root[BONGO_CAT_PATH_CAP];
    BongoCatLanguage language;
    yyjson_doc *fallback;
    yyjson_doc *active;
};

static yyjson_doc *load_locale(const char *root, BongoCatLanguage language,
    BongoCatError *error) {
    char name[32], path[BONGO_CAT_PATH_CAP];
    snprintf(name, sizeof(name), "%s.json", bongo_cat_language_name(language));
    if (!bongo_cat_path_join(path, sizeof(path), root, name)) return NULL;
    yyjson_read_err json_error = {0};
    FILE *file = bongo_cat_file_open(path, "rb");
    yyjson_doc *doc = file ? yyjson_read_fp(file, 0, NULL, &json_error) : NULL;
    if (file) fclose(file);
    if (!doc) bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
        "Cannot load locale %s: %s", path,
        json_error.msg ? json_error.msg : "cannot open file");
    return doc;
}

BongoCatI18n *bongo_cat_i18n_create(const char *root, BongoCatLanguage language,
    BongoCatError *error) {
    if (!root) return NULL;
    BongoCatI18n *value = calloc(1, sizeof(*value));
    if (!value) return NULL;
    snprintf(value->root, sizeof(value->root), "%s", root);
    value->fallback = load_locale(root, BONGO_CAT_LANG_EN_US, error);
    if (!value->fallback) {
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
    if (value->active && value->active != value->fallback) yyjson_doc_free(value->active);
    yyjson_doc_free(value->fallback);
    free(value);
}

BongoCatResult bongo_cat_i18n_reload(BongoCatI18n *value, BongoCatLanguage language,
    BongoCatError *error) {
    if (!value) return BONGO_CAT_ERROR_ARGUMENT;
    yyjson_doc *next = language == BONGO_CAT_LANG_EN_US
        ? value->fallback : load_locale(value->root, language, error);
    if (!next) return BONGO_CAT_ERROR_FORMAT;
    if (value->active && value->active != value->fallback) yyjson_doc_free(value->active);
    value->active = next;
    value->language = language;
    return BONGO_CAT_OK;
}

static yyjson_val *find_value(yyjson_doc *doc, const char *key) {
    yyjson_val *value = doc ? yyjson_doc_get_root(doc) : NULL;
    const char *cursor = key;
    while (value && cursor && *cursor) {
        const char *dot = strchr(cursor, '.');
        size_t length = dot ? (size_t)(dot - cursor) : strlen(cursor);
        char part[64];
        if (!length || length >= sizeof(part)) return NULL;
        memcpy(part, cursor, length);
        part[length] = '\0';
        value = yyjson_is_obj(value) ? yyjson_obj_get(value, part) : NULL;
        cursor = dot ? dot + 1 : NULL;
    }
    return value;
}

const char *bongo_cat_i18n_get(const BongoCatI18n *value, const char *key,
    const char *fallback) {
    if (!value || !key) return fallback;
    yyjson_val *found = find_value(value->active, key);
    if (!yyjson_is_str(found)) found = find_value(value->fallback, key);
    return yyjson_is_str(found) ? yyjson_get_str(found) : fallback;
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

static void collect_value(yyjson_val *value, uint32_t *points, size_t *count) {
    if (yyjson_is_str(value)) {
        const unsigned char *cursor = (const unsigned char *)yyjson_get_str(value);
        while (*cursor) add_point(points, count, decode_utf8(&cursor));
    } else if (yyjson_is_arr(value)) {
        size_t index, maximum; yyjson_val *item;
        yyjson_arr_foreach(value, index, maximum, item) collect_value(item, points, count);
    } else if (yyjson_is_obj(value)) {
        size_t index, maximum; yyjson_val *key, *item;
        yyjson_obj_foreach(value, index, maximum, key, item) collect_value(item, points, count);
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
    collect_value(yyjson_doc_get_root(value->active), points, &count);
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
    collect_value(yyjson_doc_get_root(value->fallback), points, &count);
    for (int language = 0; language < BONGO_CAT_LANG_COUNT; ++language) {
        if (language == BONGO_CAT_LANG_EN_US) continue;
        BongoCatError ignored = {0};
        yyjson_doc *doc = load_locale(value->root, (BongoCatLanguage)language,
            &ignored);
        if (doc) { collect_value(yyjson_doc_get_root(doc), points, &count);
            yyjson_doc_free(doc); }
    }
    return build_ranges(points, count, ranges, capacity);
}
