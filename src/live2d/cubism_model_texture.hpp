#ifndef BONGO_CAT_CUBISM_MODEL_TEXTURE_HPP
#define BONGO_CAT_CUBISM_MODEL_TEXTURE_HPP

#include "bongo_cat/image.h"
#include "cubism_texture_dimensions.hpp"
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_video.h>

namespace bongo_cat {
inline size_t gl_texture_payload_bytes(GLuint texture) {
    GLint previous = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
    glBindTexture(GL_TEXTURE_2D, texture);
    size_t bytes = 0;
    for (GLint level = 0; level < 32; ++level) {
        GLint width = 0, height = 0, compressed = GL_FALSE, size = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_WIDTH, &width);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_HEIGHT, &height);
        if (width <= 0 || height <= 0) break;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_COMPRESSED, &compressed);
        if (compressed) glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &size);
        bytes += compressed ? (size_t)size : (size_t)width * (size_t)height * 4;
        if (width == 1 && height == 1) break;
    }
    const GLenum status = glGetError();
    glBindTexture(GL_TEXTURE_2D, (GLuint)previous);
    return status == GL_NO_ERROR ? bytes : 0;
}
struct ModelTexture : TextureDimensions {
    GLuint id = 0;
    SDL_GLContext context = nullptr;
    bool direct = false;
    bool dynamic_resolution = false;
    int max_width = 0, max_height = 0;
    char digest[65]{};
    BongoCatImageAlphaMask alpha{};
    ~ModelTexture() { if (id) glDeleteTextures(1, &id); }
};
}
#endif
