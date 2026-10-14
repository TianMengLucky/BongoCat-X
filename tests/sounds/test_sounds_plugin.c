#include "test.h"
#include "bongo_cat/sounds_plugin.h"
#include <string.h>
extern const BongoCatSoundsPlugin *bongo_cat_sounds_query(uint32_t abi);
int bongo_cat_test_failures;
int main(void) {
    CHECK(bongo_cat_sounds_query(0) == NULL);
    const BongoCatSoundsPlugin *api = bongo_cat_sounds_query(BONGO_CAT_SOUNDS_ABI);
    CHECK(api && api->struct_size == sizeof(*api)); if (!api) return 1;
    void *v = api->create(); CHECK(v != NULL); if (!v) return 1;
    BongoCatSoundScene scene = {0}; scene.struct_size = sizeof(scene);
    CHECK(api->snapshot(v, &scene)); CHECK(scene.count == api->key_count());
    unsigned a = BONGO_CAT_SOUNDS_CAP, b = BONGO_CAT_SOUNDS_CAP;
    for (unsigned i = 0; i < scene.count; ++i) {
        BongoCatSoundKey key; CHECK(api->key(i, &key));
        CHECK(key.row < 6 && key.x + key.width <= 23);
        if (!strcmp(key.id, "KeyA")) a = i;
        if (!strcmp(key.id, "KeyB")) b = i;
        for (unsigned j = 0; j < i; ++j) { BongoCatSoundKey other; CHECK(api->key(j, &other)); CHECK(strcmp(key.id, other.id)); }
    }
    CHECK(a < scene.count && b < scene.count);
    BongoCatSoundBinding binding = {0}, output;
    memset(binding.asset, 'a', 64); memcpy(binding.asset + 64, ".snd", 5);
    memcpy(binding.name, "click.wav", 10);
    CHECK(api->bind(v, a, &binding)); CHECK(api->bind(v, b, &binding));
    CHECK(api->press(v, "KeyA", 1, &output)); CHECK(!strcmp(output.name, "click.wav"));
    CHECK(!api->press(v, "KeyA", 1, &output)); /* Repeat suppressed. */
    CHECK(api->press(v, "KeyB", 1, &output)); /* Independent held keys. */
    CHECK(!api->press(v, "KeyA", 0, &output)); CHECK(api->press(v, "KeyA", 1, &output));
    CHECK(!api->press(v, "Unknown", 1, &output));
    BongoCatSoundBinding bad = binding; memcpy(bad.asset, "../bad.wav", 11);
    CHECK(!api->bind(v, a, &bad));
    CHECK(api->snapshot(v, &scene)); scene.bindings[b] = bad;
    CHECK(!api->restore(v, &scene));
    CHECK(api->snapshot(v, &scene)); CHECK(!strcmp(scene.bindings[b].name, "click.wav"));
    BongoCatSoundBinding empty = {0}; CHECK(api->bind(v, a, &empty));
    CHECK(!api->press(v, "KeyA", 0, &output)); CHECK(!api->press(v, "KeyA", 1, &output));
    api->destroy(v); return bongo_cat_test_failures ? 1 : 0;
}
