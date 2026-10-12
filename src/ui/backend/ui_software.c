/* SDL's existing software renderer owns CPU rasterization and texture storage. */
#include "ui_software.h"
#include "ui_software_font.h"
#include <stddef.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <string.h>
#define POOL_KEY "BongoCat.SoftwareTexturePool"
#define RENDERER_KEY "BongoCat.SoftwareUIRenderer"
typedef struct TexturePool {
    SDL_Texture *slots[512];
    BongoCatUIFontMask *fonts[512];
    size_t frame_offset;
    int output_width, output_height;
    float logical_width, logical_height;
    SDL_FColor background;
} TexturePool;
static void SDLCALL pool_free(void *userdata, void *pointer) {
    (void)userdata; TexturePool *textures = pointer;
    for (unsigned i=0; i<512; ++i) bongo_cat_ui_font_mask_free(textures->fonts[i]);
    free(textures);
}
static TexturePool *pool(SDL_Renderer *renderer) {
    return SDL_GetPointerProperty(SDL_GetRendererProperties(renderer), POOL_KEY, NULL);
}
bool bongo_cat_ui_software_create(BongoCatUIBackend *ui) {
    ui->software_renderer = SDL_CreateRenderer(ui->window, "software");
    if (!ui->software_renderer) return false;
    TexturePool *textures = calloc(1, sizeof(*textures));
    if (!textures) return false;
    if (!SDL_SetPointerPropertyWithCleanup(SDL_GetRendererProperties(ui->software_renderer),
            POOL_KEY, textures, pool_free, NULL)) { return false; }
    return SDL_SetPointerProperty(SDL_GetWindowProperties(ui->window), RENDERER_KEY, ui->software_renderer);
}
unsigned int bongo_cat_ui_software_upload(BongoCatUIBackend *ui,
    const unsigned char *pixels, int width, int height, bool alpha_only) {
    if (!ui || !ui->software_renderer || !pixels || width <= 0 || height <= 0 ||
        width > INT_MAX / 4 || (size_t)height > SIZE_MAX / ((size_t)width * 4)) return 0;
    TexturePool *textures = pool(ui->software_renderer);
    if (!textures) return 0;
    size_t slot;
    for (slot = 0; slot < 512 && (textures->slots[slot] || textures->fonts[slot]); ++slot) {}
    if (slot == 512) return 0;
    // Match the software renderer's native alpha format. RGBA32 can otherwise
    // allocate both a source surface and a converted native texture.
    SDL_PixelFormat format = SDL_PIXELFORMAT_RGBA32;
    const SDL_PixelFormat *formats = SDL_GetPointerProperty(SDL_GetRendererProperties(ui->software_renderer),
        SDL_PROP_RENDERER_TEXTURE_FORMATS_POINTER, NULL);
    for (size_t i = 0; formats && formats[i] != SDL_PIXELFORMAT_UNKNOWN; ++i) {
        if (SDL_BYTESPERPIXEL(formats[i]) == 4 && SDL_ISPIXELFORMAT_ALPHA(formats[i])) {
            format = formats[i]; break;
        }
    }
    SDL_Texture *texture = SDL_CreateTexture(ui->software_renderer,
        format, SDL_TEXTUREACCESS_STREAMING, width, height);
    void *target = NULL; int pitch = 0;
    bool locked = texture && SDL_LockTexture(texture,NULL,&target,&pitch);
    bool ok = locked;
    unsigned char *row = alpha_only && locked ? malloc((size_t)width*4) : NULL;
    if (alpha_only) {
        if (!row) ok = false;
        for (int y = 0; ok && y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                row[x*4] = row[x*4+1] = row[x*4+2] = 255;
                row[x*4+3] = pixels[(size_t)y*width+x];
            }
            ok = SDL_ConvertPixels(width,1,SDL_PIXELFORMAT_RGBA32,row,width*4,
                format,(unsigned char *)target+(size_t)y*pitch,pitch);
        }
    } else if (ok) ok = SDL_ConvertPixels(width,height,SDL_PIXELFORMAT_RGBA32,pixels,
        width*4,format,target,pitch);
    free(row);
    if (locked) SDL_UnlockTexture(texture);
    ok = ok && SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND) &&
        SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_LINEAR);
    if (!ok) { SDL_DestroyTexture(texture); return 0; }
    textures->slots[slot] = texture;
    return (unsigned int)slot + 1;
}
unsigned int bongo_cat_ui_software_font_upload(BongoCatUIBackend *ui,
    const unsigned char *pixels, int width, int height) {
    BongoCatUIFontMask *mask=bongo_cat_ui_font_mask_create(pixels,width,height);
    if (!mask) return 0;
    const unsigned char white[]={255,255,255,255};
    unsigned id=bongo_cat_ui_software_upload(ui,white,1,1,false);
    if (!id) { bongo_cat_ui_font_mask_free(mask); return 0; }
    pool(ui->software_renderer)->fonts[id-1]=mask;
    return id;
}

void bongo_cat_ui_software_delete(BongoCatUIBackend *ui, int count, const unsigned int *ids) {
    TexturePool *textures = pool(ui->software_renderer);
    if (!textures || !ids) return;
    if (ui->software_pending) {
        ui->software_pending = false; ui->software_frame_valid = false;
        textures->frame_offset = 0;
        nk_clear(&ui->context); nk_buffer_clear(&ui->commands);
    }
    for (int i = 0; i < count; ++i) {
        if (!ids[i] || ids[i] > 512) continue;
        SDL_DestroyTexture(textures->slots[ids[i] - 1]);
        textures->slots[ids[i] - 1] = NULL;
        bongo_cat_ui_font_mask_free(textures->fonts[ids[i]-1]);
        textures->fonts[ids[i]-1]=NULL;
    }
}
/* Text and images are axis-aligned quads. SDL's blit path is much cheaper
   than rasterizing two general triangles per glyph. Validate the whole batch
   per pair of triangles; rounded shapes retain the general geometry path. */
static bool quad(const SDL_Vertex *vertices, const nk_draw_index *indices,
    SDL_FRect *source, SDL_FRect *target, SDL_FColor *color) {
    const SDL_Vertex *first = &vertices[indices[0]];
    float left = first->position.x, right = left, top = first->position.y, bottom = top;
    for (unsigned i = 0; i < 6; ++i) {
        const SDL_Vertex *v = &vertices[indices[i]];
        if (!isfinite(v->position.x) || !isfinite(v->position.y) ||
            !isfinite(v->tex_coord.x) || !isfinite(v->tex_coord.y) ||
            memcmp(&v->color, &first->color, sizeof(v->color))) return false;
        left = SDL_min(left, v->position.x); right = SDL_max(right, v->position.x);
        top = SDL_min(top, v->position.y); bottom = SDL_max(bottom, v->position.y);
    }
    if (right <= left || bottom <= top) return false;
    SDL_FPoint uv[4] = {{0}}; unsigned corners = 0;
    for (unsigned i = 0; i < 6; ++i) {
        const SDL_Vertex *v = &vertices[indices[i]];
        if ((v->position.x != left && v->position.x != right) ||
            (v->position.y != top && v->position.y != bottom)) return false;
        unsigned corner = (v->position.x == right ? 1u : 0u) | (v->position.y == bottom ? 2u : 0u);
        if ((corners & (1u << corner)) &&
            (uv[corner].x != v->tex_coord.x || uv[corner].y != v->tex_coord.y)) return false;
        uv[corner] = v->tex_coord; corners |= 1u << corner;
    }
    if (corners != 15 || uv[0].x != uv[2].x || uv[1].x != uv[3].x ||
        uv[0].y != uv[1].y || uv[2].y != uv[3].y ||
        uv[1].x < uv[0].x || uv[2].y < uv[0].y) return false;
    // Both triangles must cover the rectangle once, without repeated corners.
    float area = 0;
    for (unsigned i = 0; i < 6; i += 3) {
        SDL_FPoint a = vertices[indices[i]].position, b = vertices[indices[i+1]].position,
            c = vertices[indices[i+2]].position;
        float triangle = fabsf((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x));
        if (triangle == 0) return false;
        area += triangle;
    }
    if (fabsf(area - 2*(right-left)*(bottom-top)) > .001f) return false;
    *source = (SDL_FRect){uv[0].x, uv[0].y, uv[1].x-uv[0].x, uv[2].y-uv[0].y};
    *target = (SDL_FRect){left, top, right-left, bottom-top}; *color = first->color;
    return true;
}
bool bongo_cat_ui_software_render(BongoCatUIBackend *ui, float width, float height) {
    if (!ui || !ui->software_renderer || width <= 0 || height <= 0) return false;
    int output_width, output_height;
    if (!SDL_GetRenderOutputSize(ui->software_renderer, &output_width, &output_height) ||
        !SDL_SetRenderScale(ui->software_renderer, output_width / width, output_height / height) ||
        !SDL_SetRenderClipRect(ui->software_renderer, NULL) ||
        !SDL_SetRenderDrawBlendMode(ui->software_renderer, SDL_BLENDMODE_BLEND)) return false;
    const SDL_Vertex *vertices = ui->vertices;
    size_t count = ui->last_vertex_bytes / sizeof(*vertices), offset = 0;
    if (!count || count > INT_MAX) return false;
    TexturePool *textures = pool(ui->software_renderer);
    if (!textures) return false;
    if (ui->software_pending && (textures->output_width != output_width ||
            textures->output_height != output_height || textures->logical_width != width ||
            textures->logical_height != height)) {
        if (!SDL_SetRenderDrawColorFloat(ui->software_renderer, textures->background.r,
                textures->background.g, textures->background.b, textures->background.a) ||
                !SDL_RenderClear(ui->software_renderer)) return false;
        ui->software_pending = false;
    }
    uint64_t started = SDL_GetTicksNS();
    uint64_t budget = (uint64_t)SDL_max(1, SDL_GetNumberProperty(
        SDL_GetWindowProperties(ui->window), "BongoCat.SoftwareSliceBudgetNS", 2000000));
    bool sliced = SDL_GetBooleanProperty(SDL_GetWindowProperties(ui->window), "BongoCat.AsyncSoftwareUI", false);
    if (!ui->software_pending) {
        if (!SDL_GetRenderDrawColorFloat(ui->software_renderer, &textures->background.r,
                &textures->background.g, &textures->background.b, &textures->background.a)) return false;
        textures->output_width = output_width; textures->output_height = output_height;
        textures->logical_width = width; textures->logical_height = height;
        textures->frame_offset = 0;
        ui->software_frame_valid = false;
        ui->nonzero_alpha_vertices = 0; ui->max_alpha = 0;
        ui->last_draw_commands = 0; ui->last_draw_elements = 0;
        for (size_t i = 0; i < count; ++i) {
            if (vertices[i].color.a > 0) ui->nonzero_alpha_vertices++;
            unsigned alpha = (unsigned)SDL_clamp(vertices[i].color.a * 255, 0.0f, 255.0f);
            if (alpha > ui->max_alpha) ui->max_alpha = alpha;
        }
    }
    const struct nk_draw_command *command;
    bool ok = true;
    nk_draw_foreach(command, &ui->context, &ui->commands) {
        if (!command->elem_count) continue;
        size_t bytes = (size_t)command->elem_count * sizeof(nk_draw_index);
        unsigned id = (unsigned)command->texture.id;
        if (offset > ui->last_element_bytes || bytes > ui->last_element_bytes - offset ||
            id > 512 || (id && !textures->slots[id - 1] && !textures->fonts[id-1])) { ok = false; break; }
        if (offset + bytes <= textures->frame_offset) { offset += bytes; continue; }
        unsigned begin = (unsigned)((textures->frame_offset-offset)/sizeof(nk_draw_index));
        SDL_Rect clip = {(int)command->clip_rect.x, (int)command->clip_rect.y,
            (int)command->clip_rect.w, (int)command->clip_rect.h};
        if (!SDL_SetRenderClipRect(ui->software_renderer, &clip)) { ok = false; break; }
        while (begin < command->elem_count) {
            unsigned length = SDL_min(id == ui->font_texture ? 6u : 96u, command->elem_count-begin);
            const nk_draw_index *original = (const nk_draw_index *)((const unsigned char *)ui->elements +
                offset + (size_t)begin*sizeof(nk_draw_index));
            SDL_Vertex local[96]; int indices[96];
            bool solid = id == (unsigned)ui->null_texture.texture.id;
            for (unsigned i = 0; i < length; ++i) {
                if (original[i] >= count) { ok = false; break; }
                local[i] = vertices[original[i]]; indices[i] = (int)i;
                if (local[i].tex_coord.x != ui->null_texture.uv.x ||
                    local[i].tex_coord.y != ui->null_texture.uv.y) solid = false;
            }
            if (!ok) break;
            // A font command can coalesce null-texture shape triangles and
            // glyph quads. Consume a leading solid triangle separately so the
            // following pair of glyph triangles remains aligned.
            if (!solid && id == ui->font_texture && length >= 3) {
                bool leading_solid=true;
                for (unsigned i=0; i<3; ++i)
                    if (local[i].tex_coord.x != ui->null_texture.uv.x ||
                        local[i].tex_coord.y != ui->null_texture.uv.y) leading_solid=false;
                if (leading_solid) { length=3; solid=true; }
            }
            SDL_FRect source, target; SDL_FColor color;
            const nk_draw_index rectangle_indices[] = {0,1,2,3,4,5};
            if (solid && length == 6 && quad(local, rectangle_indices, &source, &target, &color)) {
                ok = SDL_SetRenderDrawBlendMode(ui->software_renderer, SDL_BLENDMODE_BLEND) &&
                    SDL_SetRenderDrawColorFloat(ui->software_renderer,color.r,color.g,color.b,color.a) &&
                    SDL_RenderFillRect(ui->software_renderer,&target);
            } else if (!solid && id == ui->font_texture && length == 6 &&
                    quad(local, rectangle_indices, &source, &target, &color) && source.w > 0 && source.h > 0) {
                ok = bongo_cat_ui_font_mask_draw(ui->software_renderer,textures->fonts[id-1],
                    &textures->slots[id-1],source,target,color,output_width/width,output_height/height);
            } else {
                if (!solid && id && textures->fonts[id-1]) {
                    bool empty=true;
                    for (unsigned i=0; i<length; ++i)
                        if (local[i].tex_coord.x != local[0].tex_coord.x ||
                                local[i].tex_coord.y != local[0].tex_coord.y) empty=false;
                    if (!empty) { SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
                        "Unexpected nonrectangular font geometry"); ok=false; break; }
                    // Spaces and empty glyphs have no texture area to paint.
                } else
                if (id) { SDL_SetTextureColorMod(textures->slots[id-1],255,255,255);
                    SDL_SetTextureAlphaMod(textures->slots[id-1],255); }
                ok = SDL_RenderGeometryRaw(ui->software_renderer, solid ? NULL :
                    (id ? textures->slots[id-1] : NULL), &local[0].position.x, sizeof(*local),
                    &local[0].color,sizeof(*local), &local[0].tex_coord.x,sizeof(*local),
                    (int)length,indices,(int)length,sizeof(*indices));
            }
            if (!ok || (sliced && !SDL_FlushRenderer(ui->software_renderer))) { ok = false; break; }
            if (!begin) ui->last_draw_commands++;
            begin += length; ui->last_draw_elements += length;
            textures->frame_offset += (size_t)length*sizeof(nk_draw_index);
            if (sliced && SDL_GetTicksNS()-started >= budget &&
                textures->frame_offset < ui->last_element_bytes) {
                ui->software_pending = true;
                return true; // Present only after all chunks form a complete frame.
            }
        }
        if (!ok) break;
        offset += bytes;
    }
    nk_clear(&ui->context); nk_buffer_clear(&ui->commands);
    SDL_SetRenderClipRect(ui->software_renderer,NULL);
    ui->software_pending = false;
    ui->software_frame_valid = ok;
    textures->frame_offset = 0;
    return ok;
}
