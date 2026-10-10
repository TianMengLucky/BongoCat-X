#include "overlay_internal.h"

#include <SDL3/SDL_log.h>
#include "bongo_cat/log.h"

/* Sprites draw through the shared unit-quad VAO: attribute 0 is the corner
   (y down) and attribute 1 the texel coordinate, so one program can place any
   cropped region at any canvas rect. The frame stretches horizontally in
   three slices and mirrors with the hand; glyphs use a separate readable row. */

static const char *keys_vertex_source =
    "#version 330 core\n"
    "layout(location=0) in vec2 pos; layout(location=1) in vec2 uv;\n"
    "out vec2 tex;\n"
    "uniform int dst_x,dst_y,dst_w,dst_h,reference_width,reference_height;\n"
    "uniform bool mirror,vertical_flip; uniform int src_x,src_w;\n"
    "uniform sampler2D image;\n"
    "void main(){vec2 corner=vec2(pos.x+1.,1.-pos.y)*.5;"
    "vec2 pixel=vec2(dst_x,dst_y)+corner*vec2(dst_w,dst_h);"
    "vec2 p=vec2(pixel.x/float(reference_width)*2.-1.,1.-pixel.y/float(reference_height)*2.);"
    "if(mirror)p.x=-p.x;if(vertical_flip)p.y=-p.y;gl_Position=vec4(p,0,1);tex=uv;"
    "if(src_w>0)tex.x=(float(src_x)+uv.x*float(src_w))/float(textureSize(image,0).x);}";
static const char *keys_fragment_source =
    "#version 330 core\n"
    "in vec2 tex; out vec4 color; uniform sampler2D image;\n"
    "void main(){color=texture(image,tex);if(color.a<=.001)discard;color.rgb*=color.a;}";

bool bongo_cat_overlay_keys_program(BongoCatOverlay *value) {
    if (!value) return false;
    BongoCatError error = {0};
    value->keys_program = bongo_cat_gl_program(&value->gl, keys_vertex_source,
        keys_fragment_source, &error);
    if (!value->keys_program) {
        SDL_LogWarn(BONGO_CAT_LOG_INPUT,
            "Multi-key display unavailable: %s",
            error.message[0] ? error.message : "cannot build the sprite shader");
        return false;
    }
    value->keys_src_x_location = value->gl.uniform_location(value->keys_program, "src_x");
    value->keys_src_w_location = value->gl.uniform_location(value->keys_program, "src_w");
    value->keys_dst_x_location = value->gl.uniform_location(value->keys_program, "dst_x");
    value->keys_dst_y_location = value->gl.uniform_location(value->keys_program, "dst_y");
    value->keys_dst_w_location = value->gl.uniform_location(value->keys_program, "dst_w");
    value->keys_dst_h_location = value->gl.uniform_location(value->keys_program, "dst_h");
    value->keys_mirror_location = value->gl.uniform_location(value->keys_program, "mirror");
    value->keys_flip_location = value->gl.uniform_location(value->keys_program, "vertical_flip");
    value->keys_image_location = value->gl.uniform_location(value->keys_program, "image");
    value->keys_reference_width_location = value->gl.uniform_location(value->keys_program,
        "reference_width");
    value->keys_reference_height_location = value->gl.uniform_location(value->keys_program,
        "reference_height");
    return true;
}

static void draw_region(BongoCatOverlay *value, GLuint texture, SDL_Surface *surface, int x, int y,
    int width, int height, bool mirror, int source_x, int source_width) {
    if (!texture || width <= 0 || height <= 0) return;
    if (value->native) {
        bongo_cat_overlay_native_sprite(value, surface, x, y, width, height,
            mirror, source_x, source_width);
        return;
    }
    glEnable(GL_BLEND);
    value->gl.blend_func_separate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA,
        GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    value->gl.use_program(value->keys_program);
    value->gl.uniform_1i(value->keys_mirror_location, mirror);
    value->gl.uniform_1i(value->keys_flip_location, value->vertical_flip);
    value->gl.uniform_1i(value->keys_image_location, 0);
    value->gl.uniform_1i(value->keys_src_x_location, source_x);
    value->gl.uniform_1i(value->keys_src_w_location, source_width);
    value->gl.uniform_1i(value->keys_dst_x_location, x);
    value->gl.uniform_1i(value->keys_dst_y_location, y);
    value->gl.uniform_1i(value->keys_dst_w_location, width);
    value->gl.uniform_1i(value->keys_dst_h_location, height);
    value->gl.uniform_1i(value->keys_reference_width_location, value->reference_width);
    value->gl.uniform_1i(value->keys_reference_height_location, value->reference_height);
    value->gl.active_texture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    value->gl.bind_vertex_array(value->vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    value->gl.bind_vertex_array(0);
}

static void draw_sprite(BongoCatOverlay *value, GLuint texture, SDL_Surface *surface, int x, int y,
    int width, int height, bool mirror) {
    draw_region(value, texture, surface, x, y, width, height, mirror, 0, 0);
}

static int readable_x(const BongoCatOverlay *value, int x, int width, bool mirror) {
    return mirror ? value->reference_width - x - width : x;
}

static void draw_frame(BongoCatOverlay *value, const OverlayKeySide *side,
    const OverlayKeyLayout *layout, int x, int width, int height,
    double scale, bool mirror) {
    int cap = layout->bubble_width / 3;
    int dst_cap = (int)(cap * scale);
    GLuint texture = side->keys[0].bubble;
    draw_region(value, texture, side->keys[0].bubble_image, x, layout->bubble_y, dst_cap, height,
        mirror, 0, cap);
    draw_region(value, texture, side->keys[0].bubble_image, x + dst_cap, layout->bubble_y,
        width - 2 * dst_cap, height, mirror, cap, layout->bubble_width - 2 * cap);
    draw_region(value, texture, side->keys[0].bubble_image, x + width - dst_cap, layout->bubble_y,
        dst_cap, height, mirror, layout->bubble_width - cap, cap);
}

static void draw_side(BongoCatOverlay *value, const OverlayKeySide *side,
    const OverlayKeyLayout *layout, bool mirror) {
    if (!side->count || side->fallback) return;
    int hand_x = layout->mirrored
        ? side->sprite_width - layout->hand_x - side->hand_width
        : layout->hand_x;
    draw_sprite(value, side->hand, side->hand_image, hand_x, layout->hand_y,
        side->hand_width, side->hand_height, mirror);
    int plus = layout->gap > 0 ? layout->characters_height / 2 : 0;
    int separator = plus + 2 * layout->gap;
    int row_width = ((int)side->count - 1) * separator;
    for (size_t i = 0; i < side->count; ++i)
        row_width += side->keys[i].characters_width;
    int frame_width = row_width + layout->bubble_width - layout->characters_width;
    if (frame_width < layout->bubble_width) frame_width = layout->bubble_width;
    int anchor = layout->mirrored
        ? side->sprite_width - layout->bubble_x : layout->bubble_x;
    int available = layout->mirrored ? anchor : value->reference_width - anchor;
    if (available <= 0) return;
    double scale = frame_width > available ? (double)available / frame_width : 1.0;
    int width = (int)(frame_width * scale);
    int height = (int)(layout->bubble_height * scale);
    int frame_x = layout->mirrored ? anchor - width : anchor;
    draw_frame(value, side, layout, frame_x, width, height, scale, mirror);
    /* Reflect the complete frame position, then write a left-to-right row
       inside it. Neither glyph UVs nor the reading order are reflected. */
    int x = readable_x(value, frame_x, width, mirror) +
        (int)((frame_width - row_width) * scale / 2);
    int y = layout->bubble_y +
        (int)((layout->characters_y - layout->bubble_y) * scale);
    int glyph_height = (int)(layout->characters_height * scale);
    for (size_t i = 0; i < side->count; ++i) {
        if (i) {
            x += (int)(layout->gap * scale);
            int plus_width = (int)(plus * scale);
            draw_sprite(value, side->plus, side->plus_image, x,
                y + (glyph_height - plus_width) / 2,
                plus_width, plus_width, false);
            x += plus_width + (int)(layout->gap * scale);
        }
        int glyph_width = (int)(side->keys[i].characters_width * scale);
        draw_sprite(value, side->keys[i].characters, side->keys[i].characters_image, x, y,
            glyph_width, glyph_height, false);
        x += glyph_width;
    }
}

void bongo_cat_overlay_keys_draw(BongoCatOverlay *value, bool mirror) {
    if (!bongo_cat_overlay_keys_enabled(value)) return;
    draw_side(value, &value->left_keys, &value->left_layout, mirror);
    draw_side(value, &value->right_keys, &value->right_layout, mirror);
}
