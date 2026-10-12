#ifndef BONGO_CAT_OVERLAY_INTERNAL_H
#define BONGO_CAT_OVERLAY_INTERNAL_H

#include "bongo_cat/overlay.h"
#include "bongo_cat/gl_api.h"
#include "bongo_cat/image.h"
#include "bongo_cat/input.h"
#include "mver_pointer_overlay.h"

#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_render.h>

typedef struct TextureSlot {
    char path[BONGO_CAT_PATH_CAP];
    GLuint texture;
    uint64_t used;
    BongoCatImage image;
} TextureSlot;

typedef struct OverlayNativeTexture {
    SDL_Surface *surface; /* borrowed from a live sprite */
    SDL_Texture *texture;
} OverlayNativeTexture;

#define BONGO_CAT_OVERLAY_KEYS_CAP 8
#define BONGO_CAT_OVERLAY_HELD_CAP \
    (BONGO_CAT_INPUT_KEY_STATE_CAP + BONGO_CAT_INPUT_RECOVERY_CAP)

/* Source regions and separator spacing for the shared key bubble. */
typedef struct OverlayKeyLayout {
    int hand_x, hand_y, hand_width, hand_height;
    int bubble_x, bubble_y, bubble_width, bubble_height;
    int characters_x, characters_y, characters_width, characters_height;
    int gap;
    int max_keys;
    /* x is measured from the right edge: set when the side has no
       hand-layout.json of its own and mirrors the other side's layout. */
    bool mirrored;
} OverlayKeyLayout;

typedef struct OverlayKeySprite {
    char name[BONGO_CAT_ID_CAP];
    char path[BONGO_CAT_PATH_CAP];
    GLuint bubble;
    GLuint characters;
    SDL_Surface *bubble_image, *characters_image;
    int characters_width, characters_height;
    /* Quantized mode of the glyph ink; colors the "+" separator. */
    uint32_t rgb;
} OverlayKeySprite;

typedef struct OverlayKeySide {
    OverlayKeySprite keys[BONGO_CAT_OVERLAY_KEYS_CAP];
    size_t count;
    /* Track overflow without uploading extra textures. Releasing a visible
       key promotes the next held key in press order. */
    char held[BONGO_CAT_OVERLAY_HELD_CAP][BONGO_CAT_ID_CAP];
    size_t held_count;
    int sprite_width;
    bool fallback;
    GLuint plus;
    uint32_t plus_rgb;
    GLuint hand;
    SDL_Surface *hand_image, *plus_image;
    int hand_width, hand_height;
    char hand_path[BONGO_CAT_PATH_CAP];
} OverlayKeySide;

struct BongoCatOverlay {
    bool native;
    BongoCatImage native_background;
    BongoCatImage custom_background;
    GLuint custom_texture;
    GLint backdrop_width_location, backdrop_height_location;
    int backdrop_width, backdrop_height;
    char custom_path[BONGO_CAT_PATH_CAP];
    SDL_Surface *native_canvas;
    bool native_canvas_uploaded;
    SDL_Surface *native_foreground;
    SDL_Renderer *native_renderer;
    OverlayNativeTexture native_textures[32];
    int native_foreground_geometry[6];
    bool native_foreground_mirror, native_foreground_flip;
    bool native_draw_failed;
    uint64_t native_input_revision, native_input_seen, native_foreground_revision;
    unsigned char native_pointer_key[5 * sizeof(float)];
    int native_geometry[6];
    uint32_t native_color;
    bool native_mirror, native_flip, native_opaque;
    uint64_t native_revision;
    BongoCatGL gl;
    BongoCatMverPointerOverlay *mver_pointer;
    GLuint program;
    GLint mirror_location;
    GLint vertical_flip_location;
    bool vertical_flip;
    GLint image_location;
    GLint reference_width_location;
    GLint reference_height_location;
    GLint erase_left_location;
    GLint erase_right_location;
    GLuint vao;
    GLuint vbo;
    GLuint background;
    GLuint composite;
    bool composed_cover;
    bool clean_paws;
    bool composite_dirty;
    bool model_pointer_preferred;
    int reference_width;
    int reference_height;
    /* Opaque bounding box of the background art, normalized UV with the
       origin at the top-left. Feeds the tight-window envelope so the crop
       never slices the static desk art away. */
    bool tight_uv_valid;
    float tight_uv[4];
    TextureSlot cache[4];
    GLuint left;
    GLuint right;
    GLuint effect;
    char left_name[BONGO_CAT_ID_CAP];
    char right_name[BONGO_CAT_ID_CAP];
    char background_path[BONGO_CAT_PATH_CAP];
    char left_path[BONGO_CAT_PATH_CAP];
    char right_path[BONGO_CAT_PATH_CAP];
    char effect_path[BONGO_CAT_PATH_CAP];
    char directory[BONGO_CAT_PATH_CAP];
    uint64_t clock;
    char last_input_path[BONGO_CAT_PATH_CAP];
    uint64_t input_texture_failures;
    bool multi_key_display;
    OverlayKeyLayout left_layout, right_layout;
    OverlayKeySide left_keys, right_keys;
    GLuint keys_program;
    GLint keys_dst_x_location, keys_dst_y_location;
    GLint keys_dst_w_location, keys_dst_h_location;
    GLint keys_src_x_location, keys_src_w_location;
    GLint keys_mirror_location, keys_flip_location, keys_image_location;
    GLint keys_reference_width_location, keys_reference_height_location;
};

/* Shared texture ownership for model loading and input/effect activation. */
void bongo_cat_overlay_clear_textures(BongoCatOverlay *value);
#ifdef BONGO_CAT_HAS_MODEL_PLUGINS
GLuint bongo_cat_overlay_cached_texture(BongoCatOverlay *value, const char *path);
#endif

bool bongo_cat_overlay_key_path(BongoCatOverlay *value, bool right,
    const char *name, char path[BONGO_CAT_PATH_CAP]);

/* Multi-key display: see overlay_layout.c, overlay_keys.c and
   overlay_keys_draw.c. */
void bongo_cat_overlay_layout_load(BongoCatOverlay *value);
const OverlayKeyLayout *bongo_cat_overlay_keys_layout(const BongoCatOverlay *value,
    bool right);
bool bongo_cat_overlay_keys_enabled(const BongoCatOverlay *value);
void bongo_cat_overlay_keys_clear(BongoCatOverlay *value);
void bongo_cat_overlay_keys_press(BongoCatOverlay *value, bool right,
    const char *name, const char *path);
bool bongo_cat_overlay_keys_release(BongoCatOverlay *value, bool right,
    const char *name);
GLuint bongo_cat_overlay_keys_crop_texture(BongoCatOverlay *value, const char *path,
    int x, int y, int width, int height, bool mirrored, int *out_width,
    int *out_height, int *source_width, uint32_t *rgb, SDL_Surface **surface);
GLuint bongo_cat_overlay_keys_upload(BongoCatOverlay *value,
    const unsigned char *pixels, int width, int height, SDL_Surface **surface);
uint32_t bongo_cat_overlay_keys_ink_color(const unsigned char *pixels,
    int width, int height);
bool bongo_cat_overlay_keys_bubble(BongoCatOverlay *value, const char *path,
    const OverlayKeyLayout *layout, OverlayKeySprite *entry);
bool bongo_cat_overlay_keys_program(BongoCatOverlay *value);
void bongo_cat_overlay_keys_draw(BongoCatOverlay *value, bool mirror);
void bongo_cat_overlay_native_reset(BongoCatOverlay *value);
void bongo_cat_overlay_native_forget(BongoCatOverlay *value, SDL_Surface *surface);
SDL_Surface *bongo_cat_overlay_native_image(BongoCatOverlay *value, const char *path);
void bongo_cat_overlay_native_sprite(BongoCatOverlay *value, SDL_Surface *surface,
    int x, int y, int width, int height, bool mirror, int source_x, int source_width);
void bongo_cat_overlay_native_geometry(void *userdata, unsigned primitive,
    const float *vertices, size_t count, SDL_Surface *surface);
bool bongo_cat_overlay_prepare_native_foreground(BongoCatOverlay *value,
    BongoCatRhi *rhi, int width, int height, int x, int y, int cw, int ch,
    bool mirror, bool flip);

#endif
