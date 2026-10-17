#include "mver_config_keys.h"
#include "bongo_cat/safe_import.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bongo_cat/json_dom.h"

int bongo_cat_mver_modifier_index(int code) {
    return code == 16 ? 0 : code == 17 ? 1 : code == 18 ? 2 : -1;
}

BongoCatMverKeyNames bongo_cat_mver_device_names(int code, size_t occurrence, size_t total) {
    BongoCatMverKeyNames names = {0};
    if (code >= 48 && code <= 57) {
        snprintf(names.generated, sizeof(names.generated), "Num%c", (char)code);
    } else if (code >= 65 && code <= 90) {
        snprintf(names.generated, sizeof(names.generated), "Key%c", (char)code);
    } else if (code >= 112 && code <= 135) {
        snprintf(names.generated, sizeof(names.generated), "F%d", code - 111);
    } else if (bongo_cat_mver_modifier_index(code) >= 0) {
        static const char *const modifiers[][2] = {
            {"ShiftLeft", "ShiftRight"}, {"ControlLeft", "ControlRight"},
            {"Alt", "AltGr"}
        };
        int index = bongo_cat_mver_modifier_index(code);
        if (total > 1) {
            names.items[0] = modifiers[index][occurrence ? 1 : 0];
            names.count = 1;
        } else {
            names.items[0] = modifiers[index][0];
            names.items[1] = modifiers[index][1];
            names.count = 2;
        }
        return names;
    } else {
        static const struct { int code; const char *name; } map[] = {
            {8,"Backspace"},{9,"Tab"},{13,"Return"},{19,"Pause"},{20,"CapsLock"},
            {27,"Escape"},{32,"Space"},{33,"PageUp"},{34,"PageDown"},{35,"End"},
            {36,"Home"},{37,"LeftArrow"},{38,"UpArrow"},{39,"RightArrow"},
            {40,"DownArrow"},{44,"PrintScreen"},{45,"Insert"},{46,"Delete"},
            {91,"Meta"},{92,"Meta"},{93,"Apps"},{96,"Kp0"},{97,"Kp1"},
            {98,"Kp2"},{99,"Kp3"},{100,"Kp4"},{101,"Kp5"},{102,"Kp6"},
            {103,"Kp7"},{104,"Kp8"},{105,"Kp9"},{106,"KpMultiply"},
            {107,"KpPlus"},{109,"KpMinus"},{110,"KpDecimal"},{111,"KpDivide"},
            {144,"NumLock"},{145,"ScrollLock"},{186,"Semicolon"},{187,"Equal"},
            {188,"Comma"},{189,"Minus"},{190,"Period"},{191,"Slash"},
            {192,"BackQuote"},{219,"BracketLeft"},{220,"Backslash"},
            {221,"BracketRight"},{222,"Quote"}
        };
        for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); ++i)
            if (map[i].code == code) { names.items[0] = map[i].name; names.count = 1; break; }
        if (!names.count && code > 0 && code <= 255) {
            snprintf(names.generated, sizeof(names.generated), "%d", code);
            names.count = 1;
        }
        return names;
    }
    names.count = 1;
    return names;
}

BongoCatMverKeyNames bongo_cat_mver_gamepad_names(int code) {
    static const char *map[] = {"South", "East", "West", "North", "LeftTrigger",
        "RightTrigger", "LeftTrigger2", "RightTrigger2", "LeftThumb", "RightThumb",
        "DPadLeft", "DPadRight", "DPadUp", "DPadDown", "Start", "Select"};
    BongoCatMverKeyNames names = {0};
    if (code >= 0 && (size_t)code < sizeof(map) / sizeof(map[0])) {
        names.items[0] = map[code]; names.count = 1;
    }
    return names;
}

static bool append(char *output, size_t capacity, const char *value) {
    size_t used = strlen(output), length = strlen(value);
    if (used + length >= capacity) return false;
    memcpy(output + used, value, length + 1);
    return true;
}

static const char *shortcut_key(int code, char generated[16]) {
    if (code >= 112 && code <= 135) {
        snprintf(generated, 16, "F%d", code - 111); return generated;
    }
    if (code == 187) return "=";
    if (code == 189) return "-";
    BongoCatMverKeyNames names = bongo_cat_mver_device_names(code, 0, 1);
    const char *name = names.items[0];
    if (!name && names.generated[0]) {
        snprintf(generated, 16, "%s", names.generated);
        name = generated;
    }
    if (!name || !*name || bongo_cat_mver_modifier_index(code) >= 0) return NULL;
    if (strlen(name) == 4 &&
        (strncmp(name, "Key", 3) == 0 || strncmp(name, "Num", 3) == 0)) {
        generated[0] = name[3]; generated[1] = '\0'; return generated;
    }
    if (strcmp(name, "UpArrow") == 0) return "ArrowUp";
    if (strcmp(name, "DownArrow") == 0) return "ArrowDown";
    if (strcmp(name, "LeftArrow") == 0) return "ArrowLeft";
    if (strcmp(name, "RightArrow") == 0) return "ArrowRight";
    if (strcmp(name, "Return") == 0) return "Enter";
    return name;
}

bool bongo_cat_mver_keyboard_chord(void *raw, char *output, size_t capacity) {
    return bongo_safe_mver_chord(raw, false, output, capacity);
}

static bool gamepad_chord(BongoJsonValue *row, char *output, size_t capacity) {
    if (!bongo_json_is_arr(row) || bongo_json_arr_size(row) != 1) return false;
    BongoJsonValue *key = bongo_json_arr_get_first(row);
    if (!bongo_json_is_int(key) && !bongo_json_is_uint(key)) return false;
    BongoCatMverKeyNames names = bongo_cat_mver_gamepad_names(
        (int)bongo_json_get_int(key));
    return names.count && names.items[0] &&
        snprintf(output, capacity, "Gamepad:%s", names.items[0]) > 0;
}

bool bongo_cat_mver_chord(const BongoCatImportCandidate *candidate,
    void *raw, char *output, size_t capacity) {
    BongoJsonValue *row = raw;
    BongoJsonValue *only = bongo_json_is_arr(row) && bongo_json_arr_size(row) == 1
        ? bongo_json_arr_get_first(row) : NULL;
    int code = (bongo_json_is_int(only) || bongo_json_is_uint(only))
        ? (int)bongo_json_get_int(only) : -1;
    return candidate && candidate->gamepad_buttons && code >= 0 && code <= 15
        ? gamepad_chord(row, output, capacity) :
            bongo_cat_mver_keyboard_chord(row, output, capacity);
}

bool bongo_cat_mver_shortcut_codes(const char *shortcut, char *output, size_t capacity) {
    if (!shortcut || !output || capacity < 4) return false;
    /* VK 255 is reserved. Unlike [], this preserves the row's index
       without making Mver treat an empty chord as always held. */
    if (!*shortcut) { if (capacity < 6) return false;
        snprintf(output, capacity, "[255]"); return true; }
    snprintf(output, capacity, "[");
    const char *cursor = shortcut;
    while (*cursor) {
        const char *end = strchr(cursor, '+');
        size_t length = end ? (size_t)(end - cursor) : strlen(cursor);
        char token[32];
        if (!length || length >= sizeof(token)) return false;
        memcpy(token, cursor, length); token[length] = '\0';
        int code = 0;
        static const struct { const char *name; int code; } special[] = {
            {"Control",17},{"Shift",16},{"Alt",18},{"Meta",91},
            {"ControlLeft",162},{"ControlRight",163},{"ShiftLeft",160},
            {"ShiftRight",161},{"AltGr",165},{"Left",1},{"Right",2},
            {"Middle",4},{"Back",5},{"Forward",6}
        };
        for (size_t i = 0; i < sizeof(special) / sizeof(special[0]); ++i)
            if (!strcmp(token, special[i].name)) code = special[i].code;
        for (int vk = 1; !code && vk <= 255; ++vk) {
            char generated[16]; const char *name = shortcut_key(vk, generated);
            if (name && !strcmp(token, name)) code = vk;
        }
        if (!code) return false;
        char number[16];
        snprintf(number, sizeof(number), "%s%d", cursor == shortcut ? "" : ",", code);
        if (!append(output, capacity, number)) return false;
        if (!end) break;
        cursor = end + 1;
        if (!*cursor) return false;
    }
    return append(output, capacity, "]");
}

/* Mver's audio uses Windows keys even when its visual mode uses a gamepad.
   Preserve arbitrary chords, modifier-only keys, and mouse buttons. */
bool bongo_cat_mver_sound_chord(void *raw, char *output, size_t capacity) {
    return bongo_safe_mver_chord(raw, true, output, capacity);
}
