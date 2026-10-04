#include "bongo_cat/config.h"

#include <string.h>

const char *bongo_cat_theme_name(BongoCatTheme value) {
    const char *names[] = {"auto", "light", "dark"};
    return (unsigned)value <= BONGO_CAT_THEME_DARK ? names[value] : names[0];
}

const char *bongo_cat_language_name(BongoCatLanguage value) {
    const char *names[] = {"en-US", "zh-CN", "zh-Hant", "fr-FR", "de-DE",
        "ja-JP", "ko-KR", "pt-BR", "ru-RU", "es-ES"};
    return (unsigned)value < BONGO_CAT_LANG_COUNT ? names[value] : names[0];
}

bool bongo_cat_language_parse(const char *name, BongoCatLanguage *value) {
    if (!name || !value) return false;
    if (!strcmp(name, "zh-TW")) {
        *value = BONGO_CAT_LANG_ZH_HANT;
        return true;
    }
    for (int i = 0; i < BONGO_CAT_LANG_COUNT; ++i) {
        if (strcmp(name, bongo_cat_language_name((BongoCatLanguage)i)))
            continue;
        *value = (BongoCatLanguage)i;
        return true;
    }
    return false;
}

static bool locale_part_equal(const char *text, size_t length,
    const char *expected) {
    if (length != strlen(expected)) return false;
    for (size_t i = 0; i < length; ++i) {
        unsigned char left = (unsigned char)text[i];
        unsigned char right = (unsigned char)expected[i];
        if (left >= 'A' && left <= 'Z') left += 'a' - 'A';
        if (right >= 'A' && right <= 'Z') right += 'a' - 'A';
        if (left != right) return false;
    }
    return true;
}

static bool locale_has_part(const char *text, const char *expected) {
    if (!text) return false;
    while (*text) {
        while (*text == '-' || *text == '_') text++;
        const char *end = text;
        while (*end && *end != '-' && *end != '_') end++;
        if (locale_part_equal(text, (size_t)(end - text), expected))
            return true;
        text = end;
    }
    return false;
}

static bool locale_language_is(const char *language, const char *expected) {
    if (!language) return false;
    const char *end = language;
    while (*end && *end != '-' && *end != '_') end++;
    return locale_part_equal(language, (size_t)(end - language), expected);
}

bool bongo_cat_language_from_locale(const char *language,
    const char *country, BongoCatLanguage *value) {
    if (!language || !value) return false;
    if (locale_language_is(language, "zh")) {
        bool hant = locale_has_part(language, "Hant") ||
            locale_has_part(country, "Hant");
        bool hans = locale_has_part(language, "Hans") ||
            locale_has_part(country, "Hans");
        bool traditional_region = locale_has_part(language, "TW") ||
            locale_has_part(language, "HK") ||
            locale_has_part(language, "MO") ||
            locale_has_part(country, "TW") ||
            locale_has_part(country, "HK") ||
            locale_has_part(country, "MO");
        *value = hant || (!hans && traditional_region) ?
            BONGO_CAT_LANG_ZH_HANT : BONGO_CAT_LANG_ZH_CN;
        return true;
    }
    static const struct {
        const char *code;
        BongoCatLanguage value;
    } supported[] = {{"en", BONGO_CAT_LANG_EN_US},
        {"fr", BONGO_CAT_LANG_FR_FR}, {"de", BONGO_CAT_LANG_DE_DE},
        {"ja", BONGO_CAT_LANG_JA_JP}, {"ko", BONGO_CAT_LANG_KO_KR},
        {"pt", BONGO_CAT_LANG_PT_BR}, {"ru", BONGO_CAT_LANG_RU_RU},
        {"es", BONGO_CAT_LANG_ES_ES}};
    for (size_t i = 0; i < sizeof(supported) / sizeof(supported[0]); ++i)
        if (locale_language_is(language, supported[i].code)) {
            *value = supported[i].value;
            return true;
        }
    return false;
}

const char *bongo_cat_mode_name(BongoCatModelMode value) {
    const char *names[] = {"standard", "keyboard", "gamepad"};
    return (unsigned)value <= BONGO_CAT_MODE_GAMEPAD ? names[value] : names[0];
}

const char *bongo_cat_render_backend_name(BongoCatRenderBackend value) {
    static const char *names[] = {"auto", "opengl", "vulkan"};
    return (unsigned)value < BONGO_CAT_RENDER_BACKEND_COUNT ?
        names[value] : names[BONGO_CAT_RENDER_BACKEND_AUTO];
}

bool bongo_cat_render_backend_parse(const char *name,
    BongoCatRenderBackend *value) {
    if (!name) return false;
    for (int i = 0; i < BONGO_CAT_RENDER_BACKEND_COUNT; ++i)
        if (!strcmp(name, bongo_cat_render_backend_name(
                (BongoCatRenderBackend)i))) {
            *value = (BongoCatRenderBackend)i;
            return true;
        }
    return false;
}
