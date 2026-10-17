#include "bongo_cat/hand_layout.h"
#include "bongo_cat/file.h"
#include "bongo_cat/path.h"

#include <stdio.h>
#include <string.h>

#define HAND_LAYOUT_FILE_NAME "hand-layout.json"
#define HAND_LAYOUT_TEXT_CAP 512

void bongo_cat_hand_layout_defaults(BongoSafeHandLayout *layout) {
    if (!layout) return;
    *layout = (BongoSafeHandLayout){
        .hand = {450, 629, 317, 257},
        .bubble = {47, 123, 204, 174},
        .characters = {71, 161, 168, 101},
        .gap = 12,
        .max_keys = 5,
    };
}

static bool side_folder(char *output, size_t capacity, const char *directory,
    const char *group) {
    char relative[BONGO_CAT_PATH_CAP];
    int written = snprintf(relative, sizeof(relative), "resources/%s", group);
    return written > 0 && (size_t)written < sizeof(relative) &&
        bongo_cat_path_join(output, capacity, directory, relative);
}

static bool layout_file_path(char *output, size_t capacity, const char *directory,
    const char *group) {
    char folder[BONGO_CAT_PATH_CAP];
    return side_folder(folder, sizeof(folder), directory, group) &&
        bongo_cat_path_join(output, capacity, folder, HAND_LAYOUT_FILE_NAME);
}

bool bongo_cat_hand_layout_side_exists(const char *directory, const char *group,
    bool *explicit_side) {
    if (explicit_side) *explicit_side = false;
    if (!directory || !directory[0] || !group || !group[0]) return false;
    char folder[BONGO_CAT_PATH_CAP];
    if (!side_folder(folder, sizeof(folder), directory, group) ||
        !bongo_cat_path_is_dir(folder)) return false;
    if (explicit_side) {
        char path[BONGO_CAT_PATH_CAP];
        if (layout_file_path(path, sizeof(path), directory, group) &&
            bongo_cat_path_is_file(path)) *explicit_side = true;
    }
    return true;
}

static bool serialize(const BongoSafeHandLayout *layout, char *output,
    size_t capacity) {
    int written = snprintf(output, capacity,
        "{\n"
        "  \"schemaVersion\": 1,\n"
        "  \"hand\": { \"x\": %d, \"y\": %d, \"width\": %d, \"height\": %d },\n"
        "  \"bubble\": { \"x\": %d, \"y\": %d, \"width\": %d, \"height\": %d },\n"
        "  \"characters\": { \"x\": %d, \"y\": %d, \"width\": %d, \"height\": %d },\n"
        "  \"gap\": %d,\n"
        "  \"maxKeys\": %d\n"
        "}\n",
        layout->hand.x, layout->hand.y, layout->hand.width, layout->hand.height,
        layout->bubble.x, layout->bubble.y, layout->bubble.width,
        layout->bubble.height, layout->characters.x, layout->characters.y,
        layout->characters.width, layout->characters.height, layout->gap,
        layout->max_keys);
    return written > 0 && (size_t)written < capacity;
}

static bool validated(const char *text, size_t size) {
    /* The renderer ignores documents the Rust parser rejects, so refuse to
       persist one: characters must fit inside the bubble and every field must
       stay in range. */
    BongoSafeHandLayout parsed;
    bongo_cat_hand_layout_defaults(&parsed);
    char message[256] = {0};
    int result = bongo_safe_hand_layout_parse((const unsigned char *)text, size,
        &parsed, sizeof(parsed), message, sizeof(message));
    /* Core has no logger (it never links SDL), so the caller reports the
       refusal; the parser message is kept only to satisfy the FFI contract. */
    (void)message;
    return result == 0;
}

bool bongo_cat_hand_layout_file_write(const char *directory, const char *group,
    const BongoSafeHandLayout *layout) {
    if (!directory || !directory[0] || !group || !group[0] || !layout)
        return false;
    char path[BONGO_CAT_PATH_CAP];
    if (!layout_file_path(path, sizeof(path), directory, group)) return false;
    char text[HAND_LAYOUT_TEXT_CAP];
    if (!serialize(layout, text, sizeof(text))) return false;
    if (!validated(text, strlen(text))) return false;
    char temporary[BONGO_CAT_PATH_CAP];
    int written = snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    bool ok = written > 0 && (size_t)written < sizeof(temporary);
    FILE *file = ok ? bongo_cat_file_open(temporary, "wb") : NULL;
    size_t length = strlen(text);
    ok = file && fwrite(text, 1, length, file) == length;
    if (file && fclose(file) != 0) ok = false;
    if (ok) ok = bongo_cat_file_replace(temporary, path, true);
    else if (written > 0 && (size_t)written < sizeof(temporary))
        bongo_cat_file_remove(temporary);
    return ok;
}

bool bongo_cat_hand_layout_file_ensure(const char *directory, const char *group) {
    bool explicit_side = false;
    if (!bongo_cat_hand_layout_side_exists(directory, group, &explicit_side))
        return false;
    if (explicit_side) return true;
    BongoSafeHandLayout layout;
    bongo_cat_hand_layout_defaults(&layout);
    return bongo_cat_hand_layout_file_write(directory, group, &layout);
}
