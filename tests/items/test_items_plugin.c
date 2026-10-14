#include "test.h"
#include "bongo_cat/items_plugin.h"
#include <math.h>
#include <string.h>
extern const BongoCatItemsPlugin *bongo_cat_items_query(uint32_t abi);
int bongo_cat_test_failures;
static BongoCatItem item(void) {
    BongoCatItem v = {0};
    memset(v.asset, 'a', 64); memcpy(v.asset + 64, ".img", 5);
    memcpy(v.name, "hat.png", 8);
    v.x = v.y = 50; v.scale = 25; v.opacity = 100; v.visible = v.follow_model = 1;
    return v;
}
int main(void) {
    CHECK(bongo_cat_items_query(0) == NULL);
    const BongoCatItemsPlugin *api = bongo_cat_items_query(BONGO_CAT_ITEMS_ABI);
    CHECK(api && api->struct_size == sizeof(*api));
    if (!api) return 1;
    void *instance = api->create(); CHECK(instance != NULL);
    if (!instance) return 1;
    BongoCatItemsPage page = {0};
    CHECK(!api->page(instance, &page));
    page.struct_size = sizeof(page); CHECK(api->page(instance, &page));
    CHECK(!strcmp(page.id, "items"));
    BongoCatItemScene scene = {0}; scene.struct_size = sizeof(scene);
    CHECK(api->snapshot(instance, &scene)); CHECK(scene.count == 0);
    BongoCatItem v = item(); CHECK(api->add(instance, &v));
    /* Reject path traversal and nonfinite transforms without mutating state. */
    BongoCatItem bad = v; memcpy(bad.asset, "../outside.png", 15);
    CHECK(!api->update(instance, 0, &bad));
    bad = v; bad.x = NAN; CHECK(!api->update(instance, 0, &bad));
    bad = v; bad.opacity = 101; CHECK(!api->add(instance, &bad));
    bad = v; memset(bad.name, 'x', sizeof(bad.name)); CHECK(!api->add(instance, &bad));
    bad = v; bad.follow_model = 2; CHECK(!api->add(instance, &bad));
    CHECK(api->snapshot(instance, &scene)); CHECK(scene.count == 1 && scene.items[0].x == 50);
    /* A rejected scene restore must preserve the complete previous scene. */
    scene.count = 2; scene.items[1] = v; scene.items[1].scale = 0;
    CHECK(!api->restore(instance, &scene));
    CHECK(api->snapshot(instance, &scene)); CHECK(scene.count == 1);
    for (unsigned i = 1; i < BONGO_CAT_ITEMS_CAP; ++i) {
        v.layer = (int32_t)i; CHECK(api->add(instance, &v));
    }
    CHECK(!api->add(instance, &v)); CHECK(!api->remove(instance, BONGO_CAT_ITEMS_CAP));
    CHECK(api->remove(instance, 0)); CHECK(api->snapshot(instance, &scene));
    CHECK(scene.count == BONGO_CAT_ITEMS_CAP - 1 && scene.items[0].layer == 1);
    /* Restore round-trip and instance isolation survive unload/recreation. */
    api->destroy(instance); instance = api->create(); CHECK(instance != NULL);
    CHECK(api->restore(instance, &scene));
    BongoCatItemScene restored = {0}; restored.struct_size = sizeof(restored);
    CHECK(api->snapshot(instance, &restored)); CHECK(!memcmp(&scene, &restored, sizeof(scene)));
    void *other = api->create(); CHECK(other != NULL);
    CHECK(api->snapshot(other, &restored)); CHECK(restored.count == 0);
    api->destroy(other); api->destroy(instance);
    return bongo_cat_test_failures ? 1 : 0;
}
