#include "ui_software.h"
#include "ui_font_atlas_internal.h"

#include <SDL3/SDL_opengl.h>
#include <math.h>

/* Nuklear rounds the packed atlas height to a power of two. Upload only the
   occupied rows, retaining the filter guard row and every cursor/glyph. */
static int occupied_height(struct nk_font_atlas *atlas, int height) {
    int used = atlas->custom.y + atlas->custom.h + 1;
    for (int i = 0; i < atlas->glyph_count; ++i) {
        float bottom = atlas->glyphs[i].v1 * height;
        if (!isfinite(bottom) || bottom < 0 || bottom > height) return height;
        used = SDL_max(used, (int)ceilf(bottom) + 1);
    }
    if (used <= 0 || used >= height) return height;
    float ratio = (float)height / used;
    for (int i = 0; i < atlas->glyph_count; ++i) {
        atlas->glyphs[i].v0 *= ratio;
        atlas->glyphs[i].v1 *= ratio;
    }
    atlas->tex_height = used;
    for (int i = 0; i < NK_CURSOR_COUNT; ++i)
        atlas->cursors[i].img.h = (nk_ushort)used;
    return used;
}

bool bongo_cat_ui_font_upload_atlas(BongoCatUIBackend *ui) {
    int width = 0, height = 0;
    const void *pixels = nk_font_atlas_bake(&ui->atlas, &width, &height,
        NK_FONT_ATLAS_ALPHA8);
    if (!pixels || width < 1 || height < 1) return false;
    int packed_height = height;
    height = occupied_height(&ui->atlas, height);
    if (height != packed_height) SDL_Log("Preferences font atlas trimmed: %dx%d -> %dx%d",
        width, packed_height, width, height);
    SDL_Window *window = SDL_GL_GetCurrentWindow();
    SDL_GLContext context = SDL_GL_GetCurrentContext();
    SDL_Log("Preferences font atlas upload starting: %dx%d window=%u "
        "context=%p", width, height,
        window ? (unsigned)SDL_GetWindowID(window) : 0, (void *)context);
    GLint maximum = 0;
    if (ui->software_renderer) maximum = 16384;
    else glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
    if (width > maximum || height > maximum) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "Preferences font atlas %dx%d exceeds GPU limit %d", width, height,
            maximum);
        return false;
    }
    ui->font_atlas_width = width;
    ui->font_atlas_height = height;
    SDL_Log("Preferences font atlas ready: %dx%d", width, height);
    ui->font_texture = ui->software_renderer ? bongo_cat_ui_software_font_upload(ui,pixels,width,height) :
        bongo_cat_ui_texture_upload(ui, pixels, width, height, true);
    if (!ui->font_texture) {
        ui->font_atlas_width = ui->font_atlas_height = 0;
        return false;
    }
    nk_font_atlas_end(&ui->atlas, nk_handle_id((int)ui->font_texture),
        &ui->null_texture);
    SDL_Log("Preferences font atlas upload completed: texture=%u size=%dx%d",
        ui->font_texture, width, height);
    return true;
}
