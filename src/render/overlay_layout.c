#include "overlay_internal.h"
#include "bongo_cat/file.h"
#include "bongo_cat/hand_layout.h"
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
    BongoSafeHandLayout defaults;
    bongo_cat_hand_layout_defaults(&defaults);
    layout->hand_x = defaults.hand.x;
    layout->hand_y = defaults.hand.y;
    layout->hand_width = defaults.hand.width;
    layout->hand_height = defaults.hand.height;
    layout->bubble_x = defaults.bubble.x;
    layout->bubble_y = defaults.bubble.y;
    layout->bubble_width = defaults.bubble.width;
    layout->bubble_height = defaults.bubble.height;
    layout->characters_x = defaults.characters.x;
    layout->characters_y = defaults.characters.y;
    layout->characters_width = defaults.characters.width;
    layout->characters_height = defaults.characters.height;
    layout->gap = defaults.gap;
    layout->max_keys = defaults.max_keys;
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
    OverlayKeyLayout *layout, int canvas_width, int canvas_height) {
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
    /* Older versions seeded 1000-square coordinates even for enlarged
       sprites. Recognize those default regions; custom pixel crops stay put. */
    if (canvas_width == canvas_height && canvas_width > 1000 &&
        parsed.hand.x == 450 && parsed.hand.y == 629 &&
        parsed.hand.width == 317 && parsed.hand.height == 257 &&
        parsed.bubble.x == 47 && parsed.bubble.y == 123 &&
        parsed.bubble.width == 204 && parsed.bubble.height == 174) {
        int gap = parsed.gap, max_keys = parsed.max_keys;
        bongo_cat_hand_layout_defaults_for_image(&parsed, canvas_width,
            canvas_height, false);
        parsed.gap = (int)(((int64_t)gap * canvas_width + 500) / 1000);
        parsed.max_keys = max_keys;
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
    /* With multi-key display on, an unauthored left side is seeded with a
       default hand-layout.json so the settings UI has something to edit.
       Only the left folder is seeded: a right side without its own file
       mirrors the left, and giving it one would switch its coordinates to
       the non-mirrored origin. */
    if (value->multi_key_display && directory[0])
        bongo_cat_hand_layout_file_ensure(directory, "left-keys");
    bool left = load_layout_file(directory, "left-keys", &value->left_layout,
        value->reference_width, value->reference_height);
    bool right = load_layout_file(directory, "right-keys", &value->right_layout,
        value->reference_width, value->reference_height);
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

static const char *side_group(bool right) {
    return right ? "right-keys" : "left-keys";
}

static BongoSafeHandLayout exposed_layout(const OverlayKeyLayout *layout) {
    return (BongoSafeHandLayout){
        .hand = {layout->hand_x, layout->hand_y, layout->hand_width,
            layout->hand_height},
        .bubble = {layout->bubble_x, layout->bubble_y, layout->bubble_width,
            layout->bubble_height},
        .characters = {layout->characters_x, layout->characters_y,
            layout->characters_width, layout->characters_height},
        .gap = layout->gap,
        .max_keys = layout->max_keys,
    };
}

bool bongo_cat_overlay_multi_key_layout_editable(const BongoCatOverlay *value,
    bool right) {
    if (!value || !value->directory[0]) return false;
    bool explicit_side = false;
    if (!bongo_cat_hand_layout_side_exists(value->directory, side_group(right),
        &explicit_side)) return false;
    /* A right side without its own file mirrors the left layout; editing it
       would switch the side to the non-mirrored coordinate origin, so it is
       not offered until the author supplies that side's file. */
    return !right || explicit_side;
}

bool bongo_cat_overlay_multi_key_layout_get(const BongoCatOverlay *value,
    bool right, BongoSafeHandLayout *out) {
    if (!value || !out) return false;
    *out = exposed_layout(bongo_cat_overlay_keys_layout(value, right));
    return true;
}

bool bongo_cat_overlay_multi_key_layout_save(BongoCatOverlay *value, bool right,
    const BongoSafeHandLayout *layout) {
    if (!value || !layout ||
        !bongo_cat_overlay_multi_key_layout_editable(value, right)) return false;
    if (!bongo_cat_hand_layout_file_write(value->directory, side_group(right),
        layout)) return false;
    /* Reload the written file and drop the cutouts so the host's held-input
       replay rebuilds them with the new regions, as toggling does. */
    bongo_cat_overlay_keys_clear(value);
    value->left = value->right = 0;
    value->left_name[0] = value->right_name[0] = '\0';
    value->left_path[0] = value->right_path[0] = '\0';
    ++value->native_input_revision;
    bongo_cat_overlay_layout_load(value);
    return true;
}