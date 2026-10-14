#include "ui_software.h"
#include "ui_paint_cache.h"

#include <SDL3/SDL_opengl.h>
#include <stddef.h>
#include <string.h>

struct BongoCatUIPaintTexture {
    BongoCatUIBackend *backend;
    BongoCatUIPaintKey key;
    GLuint texture;
    uint64_t used;
    size_t bytes;
};

#define PAINT_CACHE_COUNT 48
/* Effects are regenerated when evicted, so keep the cache bounded instead
   of allowing a long settings session to retain a large GPU allocation. */
#define PAINT_CACHE_BYTE_LIMIT (16u * 1024u * 1024u)

static BongoCatUIPaintTexture textures[PAINT_CACHE_COUNT];
static uint64_t use_counter;

static void release_texture(BongoCatUIPaintTexture *item) {
    if (item->texture) bongo_cat_ui_texture_delete(item->backend, 1, &item->texture);
    memset(item, 0, sizeof(*item));
}

static bool reserve_bytes(BongoCatUIBackend *backend,
    BongoCatUIPaintTexture *keep, size_t required) {
    size_t limit = backend->software_renderer ? 512u * 1024u : PAINT_CACHE_BYTE_LIMIT;
    if (required > limit) return false;
    for (;;) {
        size_t used = 0;
        BongoCatUIPaintTexture *oldest = NULL;
        for (size_t i = 0; i < PAINT_CACHE_COUNT; ++i) {
            BongoCatUIPaintTexture *item = &textures[i];
            if (item == keep || item->backend != backend || !item->texture)
                continue;
            used += item->bytes;
            bool stale = !backend->paint_frame_marker ||
                item->used < backend->paint_frame_marker;
            if (stale && (!oldest || item->used < oldest->used)) oldest = item;
        }
        if (used <= limit &&
            required <= limit - used) return true;
        if (!oldest) return false;
        release_texture(oldest);
    }
}

BongoCatUIPaintTexture *bongo_cat_ui_paint_cache_get(
    BongoCatUIBackend *backend, const BongoCatUIPaintKey *key) {
    BongoCatUIPaintTexture *empty = NULL, *oldest = NULL;
    for (size_t i = 0; i < sizeof(textures) / sizeof(textures[0]); ++i) {
        BongoCatUIPaintTexture *item = &textures[i];
        if (item->texture && item->backend == backend &&
            memcmp(&item->key, key, sizeof(*key)) == 0) {
            item->used = ++use_counter;
            return item;
        }
        if (!item->texture && !empty) empty = item;
        bool reusable = item->backend == backend &&
            (!backend->paint_frame_marker ||
                item->used < backend->paint_frame_marker);
        if (reusable && (!oldest || item->used < oldest->used)) oldest = item;
    }
    BongoCatUIPaintTexture *item = empty ? empty : oldest;
    if (!item) return NULL;
    release_texture(item);
    item->backend = backend;
    item->key = *key;
    /* Reserve before callers allocate raster pixels on either backend.
       Mask effects use R8 in GL; SDL software textures use RGBA storage. */
    bool mask = key->kind == BONGO_CAT_UI_PAINT_SHADOW ||
        key->kind == BONGO_CAT_UI_PAINT_SIDEBAR_GLOW ||
        key->kind == BONGO_CAT_UI_PAINT_ROUNDED_SURFACE ||
        ((key->kind == BONGO_CAT_UI_PAINT_RADIAL ||
          key->kind == BONGO_CAT_UI_PAINT_RADIAL_CIRCLE) &&
         key->first_color == key->second_color);
    size_t channels = mask && !backend->software_renderer ? 1u : 4u;
    if (key->width <= 0 || key->height <= 0 ||
        (size_t)key->width > SIZE_MAX / channels / (size_t)key->height ||
        !reserve_bytes(backend, item, (size_t)key->width * key->height * channels)) {
        memset(item, 0, sizeof(*item));
        return NULL;
    }
    item->used = ++use_counter;
    return item;
}

bool bongo_cat_ui_paint_cache_ready(
    const BongoCatUIPaintTexture *item) {
    return item && item->texture != 0;
}

bool bongo_cat_ui_paint_cache_upload(BongoCatUIPaintTexture *item,
    const unsigned char *pixels, bool single_channel) {
    if (!item || !pixels || item->key.width <= 0 || item->key.height <= 0)
        return false;
    size_t channels = single_channel && !item->backend->software_renderer ? 1u : 4u;
    if ((size_t)item->key.width > SIZE_MAX / (size_t)item->key.height ||
        (size_t)item->key.width * (size_t)item->key.height >
            SIZE_MAX / channels) return false;
    size_t bytes = (size_t)item->key.width * item->key.height * channels;
    if (!reserve_bytes(item->backend, item, bytes)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
            "UI paint cache limit rejected a %llu-byte texture",
            (unsigned long long)bytes);
        return false;
    }
    item->texture = bongo_cat_ui_texture_upload(item->backend, pixels, item->key.width, item->key.height, single_channel);
    if (item->texture) { item->bytes = bytes; return true; }
    return false;
}

void bongo_cat_ui_paint_cache_draw(struct nk_context *context,
    struct nk_rect bounds, const BongoCatUIPaintTexture *item,
    struct nk_color tint) {
    struct nk_image image = nk_image_id((int)item->texture);
    nk_draw_image(nk_window_get_canvas(context), bounds, &image, tint);
}

void bongo_cat_ui_paint_cache_begin_frame(BongoCatUIBackend *backend) {
    if (!backend) return;
    /* Reuse hover/animation variants across frames. LRU allocation still caps
       this cache at 48 entries / 16 MiB and protects current-frame commands. */
    backend->paint_frame_marker = use_counter + 1;
}

void bongo_cat_ui_paint_cache_trim_idle(BongoCatUIBackend *backend) {
    if (!backend) return;
    /* Commands are consumed and animations have settled. Drop effects from
       previous frames; current visible effects remain reusable. */
    for (size_t i = 0; i < PAINT_CACHE_COUNT; ++i) {
        BongoCatUIPaintTexture *item = &textures[i];
        if (item->backend == backend && item->texture &&
            backend->paint_frame_marker && item->used < backend->paint_frame_marker)
            release_texture(item);
    }
    size_t bytes = bongo_cat_ui_paint_cache_usage(backend, NULL);
    while (bytes > 4u * 1024u * 1024u) {
        BongoCatUIPaintTexture *oldest = NULL;
        for (size_t i = 0; i < PAINT_CACHE_COUNT; i++) {
            BongoCatUIPaintTexture *item = &textures[i];
            if (item->backend == backend && item->texture &&
                (!oldest || item->used < oldest->used)) oldest = item;
        }
        if (!oldest) break;
        bytes -= oldest->bytes;
        release_texture(oldest);
    }
}

size_t bongo_cat_ui_paint_cache_usage(BongoCatUIBackend *backend,
    size_t *texture_count) {
    size_t bytes = 0, count = 0;
    for (size_t i = 0; i < sizeof(textures) / sizeof(textures[0]); ++i) {
        const BongoCatUIPaintTexture *item = &textures[i];
        if (item->backend != backend || !item->texture) continue;
        bytes += item->bytes;
        count++;
    }
    if (texture_count) *texture_count = count;
    return bytes;
}

void bongo_cat_ui_paint_cache_destroy(BongoCatUIBackend *backend) {
    for (size_t i = 0; i < sizeof(textures) / sizeof(textures[0]); ++i) {
        BongoCatUIPaintTexture *item = &textures[i];
        if (item->backend != backend) continue;
        release_texture(item);
    }
    if (backend) backend->paint_frame_marker = 0;
}

void bongo_cat_ui_paint_cache_abandon(BongoCatUIBackend *backend) {
    for (size_t i = 0; i < sizeof(textures) / sizeof(textures[0]); ++i) {
        BongoCatUIPaintTexture *item = &textures[i];
        if (item->backend == backend) memset(item, 0, sizeof(*item));
    }
    if (backend) backend->paint_frame_marker = 0;
}
