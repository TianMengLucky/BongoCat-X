#include "overlay_internal.h"
#include "bongo_cat/file.h"
#include "bongo_cat/path.h"
#include "bongo_cat/safe_ffi.h"

#include <SDL3/SDL_log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Region defaults cover all poses in the 1000x1000 Selena hand sprites.
   Other artwork can override each field in hand-layout.json; sprites that
   cannot fit these regions retain their original full-image overlay. */
#define LAYOUT_FILE_CAP (256u * 1024u)

static void layout_defaults(OverlayKeyLayout *layout) {
    memset(layout, 0, sizeof(*layout));
    layout->hand_x = 450;
    layout->hand_y = 629;
    layout->hand_width = 317;
    layout->hand_height = 257;
    layout->bubble_x = 47;
    layout->bubble_y = 123;
    layout->bubble_width = 204;
    layout->bubble_height = 174;
    layout->characters_x = 71;
    layout->characters_y = 161;
    layout->characters_width = 168;
    layout->characters_height = 101;
    layout->gap = 12;
    layout->max_keys = 5;
}

static bool read_bytes(const char *path, unsigned char **bytes, size_t *size) {
    uint64_t length = 0;
    if (!bongo_cat_path_file_size(path, &length) || length == 0 ||
        length > LAYOUT_FILE_CAP) return false;
    FILE *file = bongo_cat_file_open(path, "rb");
    if (!file) return false;
    unsigned char *buffer = malloc((size_t)length);
    bool ok = buffer && fread(buffer, 1, (size_t)length, file) == (size_t)length;
    fclose(file);
    if (!ok) {
        free(buffer);
        return false;
    }
    *bytes = buffer;
    *size = (size_t)length;
    return true;
}

/* Reads resources/<group>/hand-layout.json when present. False keeps the
   caller's defaults (or the mirrored copy of the other side). */
static bool load_layout_file(const char *directory, const char *group,
    OverlayKeyLayout *layout) {
    char relative[BONGO_CAT_PATH_CAP], path[BONGO_CAT_PATH_CAP];
    snprintf(relative, sizeof(relative), "resources/%s/hand-layout.json", group);
    if (!bongo_cat_path_join(path, sizeof(path), directory, relative) ||
        !bongo_cat_path_is_file(path)) return false;
    unsigned char *bytes = NULL;
    size_t size = 0;
    if (!read_bytes(path, &bytes, &size)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
            "hand-layout.json unreadable, keeping defaults: %s", path);
        return false;
    }
    BongoSafeHandLayout parsed = {
        .hand = {layout->hand_x, layout->hand_y, layout->hand_width, layout->hand_height},
        .bubble = {layout->bubble_x, layout->bubble_y, layout->bubble_width,
            layout->bubble_height},
        .characters = {layout->characters_x, layout->characters_y,
            layout->characters_width, layout->characters_height},
        .gap = layout->gap,
        .max_keys = layout->max_keys,
    };
    char message[256] = {0};
    int result = bongo_safe_hand_layout_parse(bytes, size, &parsed, sizeof(parsed),
        message, sizeof(message));
    free(bytes);
    if (result != 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
            "hand-layout.json ignored, keeping defaults: %s: %s", path,
            message[0] ? message : "parse failure");
        return false;
    }
    layout->hand_x = parsed.hand.x;
    layout->hand_y = parsed.hand.y;
    layout->hand_width = parsed.hand.width;
    layout->hand_height = parsed.hand.height;
    layout->bubble_x = parsed.bubble.x;
    layout->bubble_y = parsed.bubble.y;
    layout->bubble_width = parsed.bubble.width;
    layout->bubble_height = parsed.bubble.height;
    layout->characters_x = parsed.characters.x;
    layout->characters_y = parsed.characters.y;
    layout->characters_width = parsed.characters.width;
    layout->characters_height = parsed.characters.height;
    layout->gap = parsed.gap;
    layout->max_keys = parsed.max_keys;
    return true;
}

void bongo_cat_overlay_layout_load(BongoCatOverlay *value) {
    if (!value) return;
    const char *directory = value->directory;
    layout_defaults(&value->left_layout);
    layout_defaults(&value->right_layout);
    bool left = load_layout_file(directory, "left-keys", &value->left_layout);
    bool right = load_layout_file(directory, "right-keys", &value->right_layout);
    /* The right side defaults to the mirrored left layout; parallel keyboards
       only provide the right folder for gamepad models. */
    value->right_layout.mirrored = true;
    if (right) value->right_layout.mirrored = false;
    else if (left) {
        value->right_layout = value->left_layout;
        value->right_layout.mirrored = true;
    }
}

const OverlayKeyLayout *bongo_cat_overlay_keys_layout(const BongoCatOverlay *value,
    bool right) {
    return right ? &value->right_layout : &value->left_layout;
}