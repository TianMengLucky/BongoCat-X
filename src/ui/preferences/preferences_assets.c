#include "ui_software.h"
#include "preferences_state.h"
#include "ui_icons.h"
#include "bongo_cat/image.h"
#include "bongo_cat/path.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <math.h>

static unsigned int load_sized(BongoCatPreferences *value, const char *name,
    int max_width, int max_height, int *width, int *height) {
    char path[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_path_join(path, sizeof(path), value->app->asset_root,
        name)) return 0;
    BongoCatError error = {0};
    if (!value->ui.software_renderer) bongo_cat_gl_clear_errors();
    unsigned int texture = bongo_cat_ui_texture_thumbnail(&value->ui, path, max_width,
        max_height, width, height, &error);
    GLenum upload_error = value->ui.software_renderer ? GL_NO_ERROR : glGetError();
    if (texture && upload_error != GL_NO_ERROR) {
        bongo_cat_ui_texture_delete(&value->ui, 1, &texture);
        texture = 0;
    }
    if (upload_error != GL_NO_ERROR)
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
            "UI asset upload failed for %s (0x%x)", name,
            (unsigned)upload_error);
    if (!texture && error.message[0])
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s", error.message);
    return texture;
}

static unsigned int load(BongoCatPreferences *value, const char *name,
    int size, int *width, int *height) {
    return load_sized(value,name,size,size,width,height);
}

static int raster_size(const BongoCatPreferences *value, int logical) {
    float scale = value->ui.raster_scale > 0.0f ? value->ui.raster_scale : 1.0f;
    return SDL_clamp((int)ceilf(logical * scale), logical, logical * 4);
}

void bongo_cat_preferences_assets_load(BongoCatPreferences *value) {
    if (!value->logo_texture)
        value->logo_texture = load(value, "bongocat.png", raster_size(value, 192),
            &value->logo_width, &value->logo_height);
    int width = 0, height = 0;
    if (!value->icon_texture)
        value->icon_texture = load(value, "ui-symbols.png", 0, &width, &height);
    /* Retrying other assets must not overwrite a successful high-DPI upload. */
    if (!value->icon_texture_hidpi) value->icon_hidpi_attempted = false;
}

void bongo_cat_preferences_qq_load(BongoCatPreferences *value) {
    if (value->qq_attempted) return;
    value->qq_attempted = true;
    value->qq_texture = load(value, "qq-group.jpg", NK_MIN(1536, raster_size(value, 768)),
        &value->qq_width, &value->qq_height);
}

void bongo_cat_preferences_icon_draw(BongoCatPreferences *value,
    struct nk_command_buffer *canvas, int icon, struct nk_rect bounds,
    struct nk_color color) {
    if (!value || !value->icon_texture || icon < 0 ||
        icon >= BONGO_CAT_UI_ICON_COUNT) return;
    bool large = bounds.w > 24.0f || bounds.h > 24.0f;
    bool needs_hidpi = large || value->ui.raster_scale > 1.05f;
    if (needs_hidpi && !value->icon_texture_hidpi && !value->icon_hidpi_attempted) {
        int width = 0, height = 0;
        value->icon_hidpi_attempted = true;
        int cell = SDL_clamp((int)ceilf(48*value->ui.raster_scale),48,96);
        value->icon_texture_hidpi = load_sized(value,"ui-symbols@4x.png",
            cell*BONGO_CAT_UI_ICON_COUNT,cell,&width,&height);
        value->icon_hidpi_cell = cell;
    }
    bool hidpi = value->icon_texture_hidpi && needs_hidpi;
    int cell = hidpi ? value->icon_hidpi_cell : 24;
    int atlas_width = cell * BONGO_CAT_UI_ICON_COUNT;
    unsigned int texture = hidpi ? value->icon_texture_hidpi : value->icon_texture;
    struct nk_image image = nk_subimage_id((int)texture, (nk_ushort)atlas_width,
        (nk_ushort)cell, nk_recti(icon * cell, 0, cell, cell));
    nk_draw_image(canvas, bounds, &image, color);
}

static void clear(BongoCatUIBackend *ui, unsigned int *texture) {
    if (*texture) bongo_cat_ui_texture_delete(ui, 1, texture);
    *texture = 0;
}

void bongo_cat_preferences_assets_clear(BongoCatPreferences *value) {
    if (!value) return;
    value->asset_retry_count = 0;
    value->asset_retry_ns = 0;
    bongo_cat_about_assets_clear(value, true);
    clear(&value->ui, &value->logo_texture);
    clear(&value->ui, &value->qq_texture);
    value->qq_attempted = false;
    clear(&value->ui, &value->icon_texture);
    clear(&value->ui, &value->icon_texture_hidpi);
    value->icon_hidpi_attempted = false;
}

void bongo_cat_preferences_assets_abandon(BongoCatPreferences *value) {
    if (!value) return;
    value->asset_retry_count = 0;
    value->asset_retry_ns = 0;
    bongo_cat_about_assets_clear(value, false);
    value->logo_texture = 0;
    value->qq_texture = 0;
    value->qq_attempted = false;
    value->icon_texture = 0;
    value->icon_texture_hidpi = 0;
    value->icon_hidpi_attempted = false;
}
