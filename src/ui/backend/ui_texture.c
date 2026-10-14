#include "ui_software.h"
#include "bongo_cat/image.h"
#include "../../media/image_internal.h"
#include <SDL3/SDL_opengl.h>
unsigned int bongo_cat_ui_software_upload(BongoCatUIBackend *, const unsigned char *, int, int, bool);
void bongo_cat_ui_software_delete(BongoCatUIBackend *, int, const unsigned int *);
unsigned int bongo_cat_ui_texture_upload(BongoCatUIBackend *ui, const unsigned char *pixels,
    int width, int height, bool alpha_only) {
    if (ui && ui->software_renderer) return bongo_cat_ui_software_upload(ui, pixels, width, height, alpha_only);
    GLuint texture = 0;
    bongo_cat_gl_clear_errors(); glGenTextures(1, &texture); glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (alpha_only) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_ONE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_ONE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_ONE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, alpha_only ? GL_R8 : GL_RGBA8, width, height, 0,
        alpha_only ? GL_RED : GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    if (glGetError() != GL_NO_ERROR) { glDeleteTextures(1, &texture); return 0; }
    return texture;
}
void bongo_cat_ui_texture_delete(BongoCatUIBackend *ui, int count, const unsigned int *textures) {
    if (ui && ui->software_renderer) bongo_cat_ui_software_delete(ui, count, textures);
    else glDeleteTextures(count, textures);
}
unsigned int bongo_cat_ui_texture_thumbnail(BongoCatUIBackend *ui, const char *path,
    int max_width, int max_height, int *width, int *height, BongoCatError *error) {
    if (!ui || !ui->software_renderer) return bongo_cat_image_texture_thumbnail(path,
        max_width, max_height, width, height, error);
    BongoCatImage source = {0}, image = {0};
    bool decoded = false;
#ifdef _WIN32
    if (max_width > 0 && max_height > 0)
        decoded = bongo_cat_image_decode_wic_responsive(path, &image,
            max_width, max_height, NULL, NULL);
#endif
    if (!decoded) {
        if (bongo_cat_image_load(path, &source, error) != BONGO_CAT_OK) return 0;
        if (!bongo_cat_image_resize_rgba_take(&source, max_width > 0 ? max_width : source.width,
                max_height > 0 ? max_height : source.height, &image, error)) {
            bongo_cat_image_free(&source); return 0;
        }
    }
    unsigned texture = bongo_cat_ui_texture_upload(ui, image.pixels, image.width, image.height, false);
    if (width) *width = image.width;
    if (height) *height = image.height;
    bongo_cat_image_free(&image);
    return texture;
}
