#include "sounds_keyboard.h"
#include <stdlib.h>
#include <string.h>
typedef struct Sounds { BongoCatSoundScene scene; uint8_t held[BONGO_CAT_SOUNDS_CAP]; } Sounds;
static uint32_t key_count(void) { return (uint32_t)(sizeof(sound_keys) / sizeof(*sound_keys)); }
static uint32_t key(uint32_t index, BongoCatSoundKey *out) {
    if (!out || index >= key_count()) return 0;
    *out = sound_keys[index]; return 1;
}
static int valid(const BongoCatSoundBinding *v) {
    if (!v || !memchr(v->asset, 0, sizeof(v->asset)) || !memchr(v->name, 0, sizeof(v->name))) return 0;
    if (!v->asset[0]) return !v->name[0];
    if (strlen(v->asset) != 68 || strcmp(v->asset + 64, ".snd") || !v->name[0]) return 0;
    for (unsigned i = 0; i < 64; ++i) if (!((v->asset[i] >= '0' && v->asset[i] <= '9') ||
        (v->asset[i] >= 'a' && v->asset[i] <= 'f'))) return 0;
    return 1;
}
static void *create(void) {
    Sounds *v = calloc(1, sizeof(*v));
    if (v) { v->scene.struct_size = sizeof(v->scene); v->scene.count = key_count(); }
    return v;
}
static void destroy(void *v) { free(v); }
static uint32_t page(void *instance, BongoCatSoundsPage *out) {
    if (!instance || !out || out->struct_size != sizeof(*out)) return 0;
    *out = (BongoCatSoundsPage){sizeof(*out), "sounds", "pages.preference.sounds.title", "Sound effects"}; return 1;
}
static uint32_t snapshot(void *instance, BongoCatSoundScene *out) {
    if (!instance || !out || out->struct_size != sizeof(*out)) return 0;
    *out = ((Sounds *)instance)->scene; return 1;
}
static uint32_t restore(void *instance, const BongoCatSoundScene *scene) {
    if (!instance || !scene || scene->struct_size != sizeof(*scene) || scene->count != key_count()) return 0;
    for (unsigned i = 0; i < scene->count; ++i) if (!valid(&scene->bindings[i])) return 0;
    ((Sounds *)instance)->scene = *scene; return 1;
}
static uint32_t bind(void *instance, uint32_t index, const BongoCatSoundBinding *binding) {
    if (!instance || index >= key_count() || !valid(binding)) return 0;
    ((Sounds *)instance)->scene.bindings[index] = *binding; return 1;
}
static uint32_t press(void *instance, const char *name, uint32_t down, BongoCatSoundBinding *out) {
    if (!instance || !name || !out || down > 1) return 0;
    Sounds *v = instance;
    *out = (BongoCatSoundBinding){0};
    for (unsigned i = 0; i < key_count(); ++i) if (!strcmp(name, sound_keys[i].id)) {
        uint32_t fresh = down && !v->held[i]; v->held[i] = (uint8_t)down;
        if (fresh && v->scene.bindings[i].asset[0]) { *out = v->scene.bindings[i]; return 1; }
        return 0;
    }
    return 0;
}
#ifdef _WIN32
#define SOUNDS_EXPORT __declspec(dllexport)
#else
#define SOUNDS_EXPORT __attribute__((visibility("default")))
#endif
SOUNDS_EXPORT const BongoCatSoundsPlugin *bongo_cat_sounds_query(uint32_t abi) {
    static const BongoCatSoundsPlugin api = {BONGO_CAT_SOUNDS_ABI, sizeof(api), create, destroy,
        page, key_count, key, snapshot, restore, bind, press};
    return abi == BONGO_CAT_SOUNDS_ABI ? &api : NULL;
}
