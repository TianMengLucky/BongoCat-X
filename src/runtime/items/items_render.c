#include "items_internal.h"
#include "bongo_cat/path.h"
#include "bongo_cat/log.h"
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
void bongo_cat_items_release_graphics(BongoCatApp *app) {
    if (!app || !app->items) return;
    BongoCatItems *v = app->items;
    SDL_Window *previous_window = SDL_GL_GetCurrentWindow();
    SDL_GLContext previous_context = SDL_GL_GetCurrentContext();
    bool has_gl = v->program != 0;
    for (unsigned i = 0; i < BONGO_CAT_ITEMS_CAP; ++i) has_gl = has_gl || v->images[i].texture != 0;
    bool gl_ready = !has_gl || SDL_GL_MakeCurrent(app->window, app->gl_context);
    if (!gl_ready) SDL_LogWarn(BONGO_CAT_LOG_LIFECYCLE, "[items] GL cleanup skipped: %s", SDL_GetError());
    for (unsigned i = 0; i < BONGO_CAT_ITEMS_CAP; ++i) {
        if (gl_ready && v->images[i].texture) glDeleteTextures(1, &v->images[i].texture);
        bongo_cat_image_free(&v->images[i].image);
        memset(&v->images[i], 0, sizeof(v->images[i]));
    }
    if (gl_ready && v->vbo) v->gl.delete_buffers(1, &v->vbo);
    if (gl_ready && v->vao) v->gl.delete_vertex_arrays(1, &v->vao);
    if (gl_ready && v->program) v->gl.delete_program(v->program);
    v->vbo = v->vao = v->program = 0;
    if (has_gl && gl_ready) (void)SDL_GL_MakeCurrent(previous_window, previous_context);
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
static void placement(BongoCatApp *app, const BongoCatItem *item, int iw, int ih,
    int width, int height, int x, int y, int cw, int ch,
    SDL_FRect *rect, double *angle, SDL_FlipMode *flip) {
    float vx = item->follow_model ? (float)x : 0;
    float vy = item->follow_model ? (float)(height - y - ch) : 0;
    float vw = item->follow_model ? (float)cw : (float)width;
    float vh = item->follow_model ? (float)ch : (float)height;
    float px = item->x / 100, py = item->y / 100;
    bool mirror = item->follow_model && app->settings.model.mirror;
    bool vertical = item->follow_model && app->settings.model.vertical_flip;
    if (mirror) px = 1 - px;
    if (vertical) py = 1 - py;
    /* Scale means percentage of the shorter viewport edge, with aspect kept. */
    float scale = fminf(vw, vh) * item->scale / 100 / (float)SDL_max(iw, ih);
    *rect = (SDL_FRect){vx + px * vw - iw * scale / 2,
        vy + py * vh - ih * scale / 2, iw * scale, ih * scale};
    *angle = mirror != vertical ? -item->rotation : item->rotation;
    *flip = (SDL_FlipMode)((mirror ? SDL_FLIP_HORIZONTAL : 0) | (vertical ? SDL_FLIP_VERTICAL : 0));
}
static void order(const BongoCatItemScene *scene, unsigned *indices) {
    for (unsigned i = 0; i < scene->count; ++i) {
        unsigned j = i;
        while (j && scene->items[indices[j - 1]].layer > scene->items[i].layer) {
            indices[j] = indices[j - 1]; --j;
        }
        indices[j] = i;
    }
}
bool bongo_cat_items_draw_native(void *userdata, SDL_Renderer *renderer, bool front,
    int width, int height, int x, int y, int cw, int ch) {
    BongoCatApp *app = userdata; BongoCatItemScene scene;
    if (!bongo_cat_items_snapshot(app, &scene)) return true;
    unsigned indices[BONGO_CAT_ITEMS_CAP]; order(&scene, indices);
    for (unsigned n = 0; n < scene.count; ++n) {
        const BongoCatItem *item = &scene.items[indices[n]];
        if (!item->visible || item->opacity <= 0 || (item->layer >= 0) != front) continue;
        int slot = image_slot(app, item->asset, true);
        if (slot < 0 || !app->items->images[slot].image.surface) continue;
        BongoCatImage *image = &app->items->images[slot].image;
        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, image->surface);
        if (!texture) return false;
        SDL_FRect rect; double angle; SDL_FlipMode flip;
        placement(app, item, image->width, image->height, width, height, x, y, cw, ch, &rect, &angle, &flip);
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
    unsigned indices[BONGO_CAT_ITEMS_CAP]; order(&scene, indices);
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_STENCIL_TEST);
    glEnable(GL_BLEND); v->gl.blend_equation(GL_FUNC_ADD);
    v->gl.blend_func_separate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    for (unsigned n = 0; n < scene.count; ++n) {
        const BongoCatItem *item = &scene.items[indices[n]];
        if (!item->visible || item->opacity <= 0 || (item->layer >= 0) != front) continue;
        int slot = image_slot(app, item->asset, false);
        if (slot < 0 || !v->images[slot].texture) continue;
        SDL_FRect rect; double angle; SDL_FlipMode flip;
        placement(app, item, v->images[slot].image.width, v->images[slot].image.height,
            width, height, x, y, cw, ch, &rect, &angle, &flip);
        float c = cosf((float)angle * .01745329252f), s = sinf((float)angle * .01745329252f);
        float vertices[4][8];
        for (unsigned j = 0; j < 4; ++j) {
            float u = (float)(j / 2), t = (float)(j % 2);
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
