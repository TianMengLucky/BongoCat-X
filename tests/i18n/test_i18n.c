#include "bongo_cat/i18n.h"
#include "bongo_cat/path.h"
#include "bongo_cat/utf8.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "bongo_cat/json_dom.h"

static bool locale_path(char *output, size_t capacity, const char *root,
    const char *name) {
    char filename[32];
    size_t length = strlen(name);
    if (length > sizeof(filename) - sizeof(".json")) return false;
    memcpy(filename, name, length);
    memcpy(filename + length, ".json", sizeof(".json"));
    return bongo_cat_path_join(output, capacity, root, filename);
}

static bool locale_encoding_valid(const char *root, const char *name) {
    char path[BONGO_CAT_PATH_CAP];
    if (!locale_path(path, sizeof(path), root, name)) return false;
    FILE *file = fopen(path, "rb");
    if (!file || fseek(file, 0, SEEK_END) != 0) {
        if (file) fclose(file);
        return false;
    }
    long end = ftell(file);
    if (end < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file); return false;
    }
    size_t length = (size_t)end;
    char *contents = malloc(length + 1);
    bool valid = contents && fread(contents, 1, length, file) == length;
    fclose(file);
    if (!valid) { free(contents); return false; }
    contents[length] = '\0';
    if (length >= 3 && (unsigned char)contents[0] == 0xef &&
        (unsigned char)contents[1] == 0xbb &&
        (unsigned char)contents[2] == 0xbf) valid = false;
    for (size_t i = 0; valid && i + 2 < length; ++i)
        if ((unsigned char)contents[i] == 0xef &&
            (unsigned char)contents[i + 1] == 0xbf &&
            (unsigned char)contents[i + 2] == 0xbd) valid = false;
    valid = valid && bongo_cat_utf8_valid(contents);
    free(contents);
    return valid;
}

static BongoJsonDoc *load(const char *root, const char *name) {
    char path[BONGO_CAT_PATH_CAP];
    if (!locale_path(path, sizeof(path), root, name)) return NULL;
    return bongo_json_read_file(path, 0, NULL, NULL);
}

static bool same_shape(BongoJsonValue *reference, BongoJsonValue *candidate,
    const char *path) {
    if (!reference || !candidate || bongo_json_get_type(reference) != bongo_json_get_type(candidate)) {
        fprintf(stderr, "Locale mismatch at %s\n", path);
        return false;
    }
    if (bongo_json_is_obj(reference) &&
        bongo_json_obj_size(reference) != bongo_json_obj_size(candidate)) {
        fprintf(stderr, "Locale key count mismatch at %s\n", path);
        return false;
    }
    if (!bongo_json_is_obj(reference)) return true;
    size_t index, count; BongoJsonValue *key, *value;
    bongo_json_obj_foreach(reference, index, count, key, value) {
        const char *name = bongo_json_get_str(key);
        BongoJsonValue *next = bongo_json_obj_get(candidate, name);
        char child[BONGO_CAT_PATH_CAP];
        size_t path_length = strlen(path), name_length = strlen(name);
        size_t separator = path_length ? 1 : 0;
        size_t available = path_length < sizeof(child)
            ? sizeof(child) - path_length : 0;
        if (separator >= available || name_length >= available - separator) {
            fprintf(stderr, "Locale key path is too long\n");
            return false;
        }
        memcpy(child, path, path_length);
        if (separator) child[path_length] = '.';
        memcpy(child + path_length + separator, name, name_length + 1);
        if (!same_shape(value, next, child)) return false;
    }
    return true;
}

/* i18n_get silently falls back to English, so presence checks must look at
   the locale document itself to be able to report a gap at all. */
static bool lookup_path(BongoJsonValue *value, const char *dotted) {
    const char *cursor = dotted;
    while (value && cursor && *cursor) {
        const char *dot = strchr(cursor, '.');
        size_t length = dot ? (size_t)(dot - cursor) : strlen(cursor);
        char part[64];
        if (!length || length >= sizeof(part)) return false;
        memcpy(part, cursor, length);
        part[length] = '\0';
        value = bongo_json_is_obj(value) ? bongo_json_obj_get(value, part) : NULL;
        cursor = dot ? dot + 1 : NULL;
    }
    return value != NULL;
}

static bool includes(const uint32_t *ranges, uint32_t point) {
    for (size_t i = 0; ranges[i] && ranges[i + 1]; i += 2)
        if (point >= ranges[i] && point <= ranges[i + 1]) return true;
    return false;
}

static uint32_t next_utf8(const unsigned char **cursor) {
    const unsigned char *value = *cursor;
    uint32_t point = *value++;
    int remaining = 0;
    if (point < 0x80) { *cursor = value; return point; }
    if ((point & 0xe0) == 0xc0) { point &= 0x1f; remaining = 1; }
    else if ((point & 0xf0) == 0xe0) { point &= 0x0f; remaining = 2; }
    else if ((point & 0xf8) == 0xf0) { point &= 0x07; remaining = 3; }
    else { *cursor = value; return 0; }
    while (remaining--) {
        if ((*value & 0xc0) != 0x80) { *cursor = value; return 0; }
        point = (point << 6) | (*value++ & 0x3f);
    }
    *cursor = value;
    return point;
}

static bool covers_value(const uint32_t *ranges, BongoJsonValue *value) {
    if (bongo_json_is_str(value)) {
        const unsigned char *text = (const unsigned char *)bongo_json_get_str(value);
        while (*text) {
            uint32_t point = next_utf8(&text);
            if (point >= 0x20 && !includes(ranges, point)) return false;
        }
    } else if (bongo_json_is_arr(value)) {
        size_t index, count; BongoJsonValue *item;
        bongo_json_arr_foreach(value, index, count, item)
            if (!covers_value(ranges, item)) return false;
    } else if (bongo_json_is_obj(value)) {
        size_t index, count; BongoJsonValue *key, *item;
        bongo_json_obj_foreach(value, index, count, key, item)
            if (!covers_value(ranges, item)) return false;
    }
    return true;
}

static bool contains_replacement(BongoJsonValue *value) {
    if (bongo_json_is_str(value)) {
        const unsigned char *text = (const unsigned char *)bongo_json_get_str(value);
        while (*text) if (next_utf8(&text) == 0xfffd) return true;
    } else if (bongo_json_is_arr(value)) {
        size_t index, count; BongoJsonValue *item;
        bongo_json_arr_foreach(value, index, count, item)
            if (contains_replacement(item)) return true;
    } else if (bongo_json_is_obj(value)) {
        size_t index, count; BongoJsonValue *key, *item;
        bongo_json_obj_foreach(value, index, count, key, item)
            if (contains_replacement(item)) return true;
    }
    return false;
}

int main(void) {
    char root[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_path_join(root, sizeof(root), BONGO_CAT_NATIVE_SOURCE_DIR,
        "resources/assets/locales")) return 1;
    BongoJsonDoc *reference = load(root, "zh-CN");
    if (!reference) return 1;
    const char *required_ui_keys[] = {
        "pages.preference.general.labels.runAsAdmin",
        "pages.preference.general.hints.runAsAdmin",
        "pages.preference.general.hints.runAsAdminHelp",
        "pages.preference.general.hints.runAsAdminFailed",
        "pages.preference.general.hints.autostartFailed",
        "pages.preference.model.hints.deleteFailed",
        "pages.preference.model.hints.importFailed",
        "native.preferencesWindowTitle",
        "native.startup.failed",
        "native.startup.detail",
        "native.startup.diagnosticLog",
        "native.support.website",
        "native.support.openLogs",
        "native.support.starOnGitHub",
        "native.support.by"};
    const uint32_t expected[] = {'A', 0x4e2d, 0x8a2d, 0x00e7, 0x00fc,
        0x65e5, 0xd55c, 0x00ea, 0x0420, 0x00f1};
    for (int language = 0; language < BONGO_CAT_LANG_COUNT; ++language) {
        const char *name = bongo_cat_language_name((BongoCatLanguage)language);
        /* zh-CN and en-US are the maintained reference locales. The app falls
           back to English per key, so a gap in any other locale only costs a
           little polish, while a gap here hides the feature everywhere. */
        bool canonical = language == BONGO_CAT_LANG_ZH_CN ||
            language == BONGO_CAT_LANG_EN_US;
        if (!locale_encoding_valid(root, name)) {
            fprintf(stderr, "Invalid UTF-8 locale encoding: %s\n", name);
            return 8;
        }
        BongoJsonDoc *document = load(root, name);
        if (!document) {
            fprintf(stderr, "Cannot load locale: %s\n", name);
            return 1;
        }
        if (contains_replacement(bongo_json_doc_get_root(document))) {
            fprintf(stderr, "Invalid replacement character in locale: %s\n", name);
            return 2;
        }
        if (!same_shape(bongo_json_doc_get_root(reference),
                bongo_json_doc_get_root(document), "")) {
            if (canonical) return 2;
            fprintf(stderr, "Warning: locale %s does not match the zh-CN key "
                "shape; missing entries fall back to English\n", name);
        }
        BongoCatError error = {0};
        BongoCatI18n *i18n = bongo_cat_i18n_create(root, (BongoCatLanguage)language, &error);
        uint32_t ranges[2048];
        if (!i18n || bongo_cat_i18n_glyph_ranges(i18n, ranges, 2048) < 3 ||
            ranges[0] != 0x20 || !includes(ranges, expected[language]) ||
            !covers_value(ranges, bongo_json_doc_get_root(document))) {
            fprintf(stderr, "Missing U+%04X for %s\n", expected[language], name);
            return 3;
        }
        for (size_t i = 0; i < sizeof(required_ui_keys) /
            sizeof(required_ui_keys[0]); ++i) {
            if (lookup_path(bongo_json_doc_get_root(document),
                    required_ui_keys[i])) continue;
            if (canonical) {
                fprintf(stderr, "Missing required translation %s for %s\n",
                    required_ui_keys[i], name);
                return 7;
            }
            fprintf(stderr, "Warning: locale %s misses %s; using English\n",
                name, required_ui_keys[i]);
        }
        bongo_cat_i18n_destroy(i18n);
        bongo_json_doc_free(document);
    }
    BongoCatError error = {0};
    BongoCatI18n *all = bongo_cat_i18n_create(root,
        BONGO_CAT_LANG_EN_US, &error);
    uint32_t all_ranges[8192];
    const uint32_t menu_points[] = {0x7b80, 0x9ad4, 0x00e7, 0x00fc, 0x65e5,
        0xd55c, 0x00ea, 0x0420, 0x00f1};
    if (!all || bongo_cat_i18n_all_glyph_ranges(all, all_ranges, 8192) < 3)
        return 4;
    for (size_t i = 0; i < sizeof(menu_points) / sizeof(menu_points[0]); ++i)
        if (!includes(all_ranges, menu_points[i])) return 5;
    for (int language = 0; language < BONGO_CAT_LANG_COUNT; ++language) {
        BongoJsonDoc *document = load(root,
            bongo_cat_language_name((BongoCatLanguage)language));
        if (!document || !covers_value(all_ranges, bongo_json_doc_get_root(document)))
            return 6;
        bongo_json_doc_free(document);
    }
    bongo_cat_i18n_destroy(all);
    bongo_json_doc_free(reference);
    puts("i18n smoke passed");
    return 0;
}
