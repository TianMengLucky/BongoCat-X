#include "test.h"
#include "bongo_cat/file.h"
#include "bongo_cat/hand_layout.h"
#include "bongo_cat/path.h"
#include "bongo_cat/safe_ffi.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define HAND_LAYOUT_ROOT "hand-layout-test"
#define HAND_LAYOUT_FILE HAND_LAYOUT_ROOT "/resources/left-keys/hand-layout.json"

/* Parses the written file with the same Rust parser the renderer uses, so the
   test asserts the persisted bytes, not just the in-memory struct. */
static bool read_layout(const char *path, BongoSafeHandLayout *layout) {
    bongo_cat_hand_layout_defaults(layout);
    uint64_t length = 0;
    if (!bongo_cat_path_file_size(path, &length) || length == 0 ||
        length > 4096) return false;
    unsigned char buffer[4096];
    FILE *file = bongo_cat_file_open(path, "rb");
    if (!file) return false;
    size_t read = fread(buffer, 1, (size_t)length, file);
    fclose(file);
    if (read != (size_t)length) return false;
    char message[256] = {0};
    return bongo_safe_hand_layout_parse(buffer, read, layout, sizeof(*layout),
        message, sizeof(message)) == 0;
}

void test_hand_layout_file(void) {
    BongoSafeHandLayout defaults;
    bongo_cat_hand_layout_defaults(&defaults);
    CHECK(defaults.hand.x == 450 && defaults.hand.y == 629);
    CHECK(defaults.hand.width == 317 && defaults.hand.height == 257);
    CHECK(defaults.bubble.x == 47 && defaults.bubble.y == 123);
    CHECK(defaults.bubble.width == 204 && defaults.bubble.height == 174);
    CHECK(defaults.characters.x == 71 && defaults.characters.y == 161);
    CHECK(defaults.characters.width == 168 && defaults.characters.height == 101);
    CHECK(defaults.gap == 12);
    CHECK(defaults.max_keys == 5);

    bongo_cat_file_remove(HAND_LAYOUT_FILE);
    CHECK(bongo_cat_path_create_directory(HAND_LAYOUT_ROOT));
    char resources[BONGO_CAT_PATH_CAP];
    CHECK(bongo_cat_path_join(resources, sizeof(resources), HAND_LAYOUT_ROOT,
        "resources"));
    CHECK(bongo_cat_path_create_directory(resources));
    char left[BONGO_CAT_PATH_CAP];
    CHECK(bongo_cat_path_join(left, sizeof(left), resources, "left-keys"));
    CHECK(bongo_cat_path_create_directory(left));

    bool explicit_side = false;
    CHECK(bongo_cat_hand_layout_side_exists(HAND_LAYOUT_ROOT, "left-keys",
        &explicit_side));
    CHECK(!explicit_side);
    /* A folder the model does not ship cannot be seeded. */
    CHECK(!bongo_cat_hand_layout_side_exists(HAND_LAYOUT_ROOT, "right-keys",
        NULL));
    CHECK(!bongo_cat_hand_layout_file_ensure(HAND_LAYOUT_ROOT, "right-keys"));

    /* Enabling the feature seeds the left folder with the built-in defaults. */
    CHECK(bongo_cat_hand_layout_file_ensure(HAND_LAYOUT_ROOT, "left-keys"));
    BongoSafeHandLayout loaded;
    CHECK(read_layout(HAND_LAYOUT_FILE, &loaded));
    CHECK(!memcmp(&loaded, &defaults, sizeof(defaults)));
    explicit_side = false;
    CHECK(bongo_cat_hand_layout_side_exists(HAND_LAYOUT_ROOT, "left-keys",
        &explicit_side));
    CHECK(explicit_side);

    /* Authored values round-trip, and a second ensure leaves them alone. */
    BongoSafeHandLayout custom = defaults;
    custom.hand.x = 10;
    custom.hand.width = 100;
    custom.bubble = (BongoSafeHandRect){1, 2, 300, 200};
    custom.characters = (BongoSafeHandRect){5, 6, 100, 80};
    custom.gap = 6;
    custom.max_keys = 3;
    CHECK(bongo_cat_hand_layout_file_write(HAND_LAYOUT_ROOT, "left-keys",
        &custom));
    CHECK(read_layout(HAND_LAYOUT_FILE, &loaded));
    CHECK(!memcmp(&loaded, &custom, sizeof(custom)));
    CHECK(bongo_cat_hand_layout_file_ensure(HAND_LAYOUT_ROOT, "left-keys"));
    CHECK(read_layout(HAND_LAYOUT_FILE, &loaded));
    CHECK(!memcmp(&loaded, &custom, sizeof(custom)));

    /* Characters outside the bubble and an empty rectangle are both rejected,
       and the previous file survives the failed write. */
    BongoSafeHandLayout invalid = custom;
    invalid.characters.x = 0;
    invalid.characters.width = 4;
    CHECK(!bongo_cat_hand_layout_file_write(HAND_LAYOUT_ROOT, "left-keys",
        &invalid));
    invalid = custom;
    invalid.hand.height = 0;
    CHECK(!bongo_cat_hand_layout_file_write(HAND_LAYOUT_ROOT, "left-keys",
        &invalid));
    invalid = custom;
    invalid.max_keys = 9;
    CHECK(!bongo_cat_hand_layout_file_write(HAND_LAYOUT_ROOT, "left-keys",
        &invalid));
    CHECK(read_layout(HAND_LAYOUT_FILE, &loaded));
    CHECK(!memcmp(&loaded, &custom, sizeof(custom)));

    CHECK(bongo_cat_file_remove(HAND_LAYOUT_FILE));
    CHECK(bongo_cat_path_remove(left));
    CHECK(bongo_cat_path_remove(resources));
    CHECK(bongo_cat_path_remove(HAND_LAYOUT_ROOT));
}
