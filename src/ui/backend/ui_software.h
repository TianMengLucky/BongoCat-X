#ifndef BONGO_CAT_UI_SOFTWARE_H
#define BONGO_CAT_UI_SOFTWARE_H
#include "ui_backend.h"
bool bongo_cat_ui_software_create(BongoCatUIBackend *ui);
unsigned int bongo_cat_ui_software_font_upload(BongoCatUIBackend *, const unsigned char *, int, int);
bool bongo_cat_ui_software_render(BongoCatUIBackend *ui, float width, float height);
unsigned int bongo_cat_ui_texture_upload(BongoCatUIBackend *ui, const unsigned char *pixels, int width, int height, bool alpha_only);
void bongo_cat_ui_texture_delete(BongoCatUIBackend *ui, int count, const unsigned int *textures);
unsigned int bongo_cat_ui_texture_thumbnail(BongoCatUIBackend *ui, const char *path, int max_width, int max_height, int *width, int *height, BongoCatError *error);
#endif
