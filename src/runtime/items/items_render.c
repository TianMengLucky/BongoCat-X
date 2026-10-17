#include "items_internal.h"
#include "bongo_cat/path.h"
#include "bongo_cat/log.h"
#include "bongo_cat/memory_policy.h"
#include <SDL3/SDL_opengl.h>
#include <math.h>
#include <string.h>
/* Same straight-alpha texture / premultiplied output convention as overlays. */
static const char *vertex_source = "#version 330 core\n"
    "layout(location=0) in vec2 pos;layout(location=1) in vec2 uv;"
    "layout(location=2) in vec4 tint;out vec2 tex;out vec4 color;"
    "void main(){gl_Position=vec4(pos,0,1);tex=uv;color=tint;}";
static const char *fragment_source = "#version 330 core\n"
    "in vec2 tex;in vec4 color;out vec4 outputColor;uniform sampler2D image;"
    "void main(){vec4 v=texture(image,tex)*color;v.rgb*=v.a;outputColor=v;}";
static bool program(BongoCatItems *v) {
    if (v->program) return true;
    BongoCatError error = {0};
    if (!bongo_cat_gl_load(&v->gl, &error)) return false;
    v->program = bongo_cat_gl_program(&v->gl, vertex_source, fragment_source, &error);
    if (!v->program) return false;
    v->gl.gen_vertex_arrays(1, &v->vao); v->gl.bind_vertex_array(v->vao);
    v->gl.gen_buffers(1, &v->vbo); v->gl.bind_buffer(GL_ARRAY_BUFFER, v->vbo);
    for (unsigned i = 0; i < 3; ++i) {
        v->gl.enable_attribute(i);
        v->gl.attribute_pointer(i, i == 2 ? 4 : 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
            (const void *)(uintptr_t)(i * 2 * sizeof(float)));
    }
    v->gl.bind_vertex_array(0);
    return true;
}
static void release_graphics(BongoCatApp *app, const BongoCatItemScene *scene) {
    if (!app || !app->items) return;
    BongoCatItems *v = app->items;
    bool keep[BONGO_CAT_ITEMS_CAP] = {0};
    for (unsigned i = 0; scene && i < BONGO_CAT_ITEMS_CAP; ++i)
        for (unsigned j = 0; j < scene->count; ++j)
            if (scene->items[j].visible && scene->items[j].opacity > 0 &&
                !strcmp(v->images[i].asset, scene->items[j].asset)) keep[i] = true;
    SDL_Window *previous_window = SDL_GL_GetCurrentWindow();
    SDL_GLContext previous_context = SDL_GL_GetCurrentContext();
    bool has_gl = !scene && (v->program || v->vao || v->vbo);
    for (unsigned i = 0; i < BONGO_CAT_ITEMS_CAP; ++i)
        has_gl = has_gl || (!keep[i] && v->images[i].texture != 0);
    if (has_gl && (!app->window || !app->gl_context ||
        !SDL_GL_MakeCurrent(app->window, app->gl_context))) {
        SDL_LogWarn(BONGO_CAT_LOG_LIFECYCLE, "[items] GL cleanup skipped: %s", SDL_GetError());
        return; /* Keep owned handles so a later cleanup can retry. */
    }
    bool released = false;
    for (unsigned i = 0; i < BONGO_CAT_ITEMS_CAP; ++i) {
        if (keep[i]) continue;
        released = released || v->images[i].image.pixels || v->images[i].texture;
        if (v->images[i].texture) glDeleteTextures(1, &v->images[i].texture);
        bongo_cat_image_free(&v->images[i].image);
        memset(&v->images[i], 0, sizeof(v->images[i]));
    }
    if (!scene) {
        if (v->vbo) v->gl.delete_buffers(1, &v->vbo);
        if (v->vao) v->gl.delete_vertex_arrays(1, &v->vao);
        if (v->program) v->gl.delete_program(v->program);
        v->vbo = v->vao = v->program = 0;
    }
    if (has_gl) (void)SDL_GL_MakeCurrent(previous_window, previous_context);
    if (released) bongo_cat_memory_policy_temporary_released();
}
void bongo_cat_items_release_graphics(BongoCatApp *app) {
    release_graphics(app, NULL);
}
void bongo_cat_items_prune_graphics(BongoCatApp *app) {
    BongoCatItemScene scene;
    /* Runs on the main thread when a new scene is published, including empty
       scenes that will not invoke either draw callback again. */
    if (bongo_cat_items_snapshot(app, &scene))
        release_graphics(app, scene.count ? &scene : NULL);
}
static int image_slot(BongoCatApp *app, const char *asset, bool native) {
    BongoCatItems *v = app->items;
    int slot = -1;
    for (unsigned i = 0; i < BONGO_CAT_ITEMS_CAP; ++i) {
        if (!strcmp(v->images[i].asset, asset)) return (int)i;
        if (!v->images[i].asset[0] && slot < 0) slot = (int)i;
    }
    if (slot < 0) return -1;
    char directory[BONGO_CAT_PATH_CAP], path[BONGO_CAT_PATH_CAP];
    uint64_t size = 0; int width = 0, height = 0;
    BongoCatError error = {0};
    SDL_strlcpy(v->images[slot].asset, asset, sizeof(v->images[slot].asset));
    if (!bongo_cat_path_join(directory, sizeof(directory), app->data_root, "items") ||
        !bongo_cat_path_join(path, sizeof(path), directory, asset) ||
        !bongo_cat_path_file_size(path, &size) || size > 64u * 1024u * 1024u ||
        !bongo_cat_image_info(path, &width, &height) || width <= 0 || height <= 0 ||
        (uint64_t)width * height > 16777216) goto failed;
    if (native) {
        BongoCatImage image = {0};
        if (bongo_cat_image_load(path, &image, &error) != BONGO_CAT_OK) goto failed;
        bool ok = bongo_cat_image_resize_rgba_take(&image, 1024, 1024, &v->images[slot].image, &error);
        bongo_cat_image_free(&image);
        if (!ok) goto failed;
        BongoCatImage *out = &v->images[slot].image;
        if (!out->surface) out->surface = SDL_CreateSurfaceFrom(out->width, out->height,
            SDL_PIXELFORMAT_RGBA32, out->pixels, out->width * 4);
        if (!out->surface) goto failed;
    } else {
        v->images[slot].texture = bongo_cat_image_texture_thumbnail(path, 1024, 1024,
            &v->images[slot].image.width, &v->images[slot].image.height, &error);
        if (!v->images[slot].texture) goto failed;
    }
    return slot;
failed:
    SDL_LogWarn(BONGO_CAT_LOG_LIFECYCLE, "[items] Cannot load %s: %s", asset, error.message);
    return slot; /* Remember the failure; do not reopen the file every frame. */
}
const BongoCatItemsRenderPlugin *bongo_cat_items_render_query(SDL_SharedObject *library) {
    union { SDL_FunctionPointer function; BongoCatItemsRenderQuery query; } entry;
    entry.function = SDL_LoadFunction(library, "bongo_cat_items_render_query_v1");
    const BongoCatItemsRenderPlugin *api = entry.function ? entry.query(1) : NULL;
    return api && api->abi_version == 1 && api->struct_size == sizeof(*api) &&
        api->order && api->placement ? api : NULL;
}
static bool placement(BongoCatApp *app, const BongoCatItem *item, int iw, int ih,
    int width, int height, int x, int y, int cw, int ch,
    SDL_FRect *rect, double *angle, SDL_FlipMode *flip) {
    BongoCatItemViewport viewport = {width, height, x, y, cw, ch,
        app->settings.model.mirror, app->settings.model.vertical_flip};
    BongoCatItemPlacement value = {0};
    if (!app->items->render->placement(item, &viewport, iw, ih, &value)) return false;
    *rect = (SDL_FRect){value.x, value.y, value.width, value.height};
    *angle = value.angle; *flip = (SDL_FlipMode)value.flip;
    return true;
}
bool bongo_cat_items_draw_native(void *userdata, SDL_Renderer *renderer, bool front,
    int width, int height, int x, int y, int cw, int ch) {
    BongoCatApp *app = userdata; BongoCatItemScene scene;
    if (!bongo_cat_items_snapshot(app, &scene)) return true;
    uint32_t indices[BONGO_CAT_ITEMS_CAP];
    uint32_t count = app->items->render->order(&scene, front, indices, BONGO_CAT_ITEMS_CAP);
    if (count > BONGO_CAT_ITEMS_CAP) return true;
    for (unsigned n = 0; n < count; ++n) {
        if (indices[n] >= scene.count) continue;
        const BongoCatItem *item = &scene.items[indices[n]];
        int slot = image_slot(app, item->asset, true);
        if (slot < 0 || !app->items->images[slot].image.surface) continue;
        BongoCatImage *image = &app->items->images[slot].image;
        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, image->surface);
        if (!texture) return false;
        SDL_FRect rect; double angle; SDL_FlipMode flip;
        if (!placement(app, item, image->width, image->height, width, height, x, y, cw, ch, &rect, &angle, &flip)) {
            SDL_DestroyTexture(texture); continue;
        }
        bool ok = SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND) &&
            SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR) &&
            SDL_SetTextureAlphaModFloat(texture, item->opacity / 100) &&
            SDL_RenderTextureRotated(renderer, texture, NULL, &rect, angle, NULL, flip);
        SDL_DestroyTexture(texture);
        if (!ok) return false;
    }
    return true;
}
void bongo_cat_items_draw_gl(BongoCatApp *app, bool front, int width, int height,
    int x, int y, int cw, int ch) {
    BongoCatItemScene scene;
    if (width <= 0 || height <= 0 || !bongo_cat_items_snapshot(app, &scene) || !scene.count) return;
    BongoCatItems *v = app->items;
    if (!program(v)) return;
    uint32_t indices[BONGO_CAT_ITEMS_CAP];
    uint32_t count = v->render->order(&scene, front, indices, BONGO_CAT_ITEMS_CAP);
    if (count > BONGO_CAT_ITEMS_CAP) return;
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_STENCIL_TEST);
    glEnable(GL_BLEND); v->gl.blend_equation(GL_FUNC_ADD);
    v->gl.blend_func_separate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    for (unsigned n = 0; n < count; ++n) {
        if (indices[n] >= scene.count) continue;
        const BongoCatItem *item = &scene.items[indices[n]];
        int slot = image_slot(app, item->asset, false);
        if (slot < 0 || !v->images[slot].texture) continue;
        SDL_FRect rect; double angle; SDL_FlipMode flip;
        if (!placement(app, item, v->images[slot].image.width, v->images[slot].image.height,
            width, height, x, y, cw, ch, &rect, &angle, &flip)) continue;
        float c = cosf((float)angle * .01745329252f), s = sinf((float)angle * .01745329252f);
        float vertices[4][8];
        for (unsigned j = 0; j < 4; ++j) {
            float u = j >= 2 ? 1.0f : 0.0f, t = (j & 1u) ? 1.0f : 0.0f;
            float dx = (u - .5f) * rect.w, dy = (t - .5f) * rect.h;
            vertices[j][0] = (rect.x + rect.w / 2 + c * dx - s * dy) / width * 2 - 1;
            vertices[j][1] = 1 - (rect.y + rect.h / 2 + s * dx + c * dy) / height * 2;
            vertices[j][2] = (flip & SDL_FLIP_HORIZONTAL) ? 1 - u : u;
            vertices[j][3] = (flip & SDL_FLIP_VERTICAL) ? 1 - t : t;
            vertices[j][4] = vertices[j][5] = vertices[j][6] = 1;
            vertices[j][7] = item->opacity / 100;
        }
        v->gl.use_program(v->program);
        v->gl.uniform_1i(v->gl.uniform_location(v->program, "image"), 0);
        v->gl.active_texture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, v->images[slot].texture);
        v->gl.bind_vertex_array(v->vao); v->gl.bind_buffer(GL_ARRAY_BUFFER, v->vbo);
        v->gl.buffer_data(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); v->gl.bind_vertex_array(0);
    }
}
