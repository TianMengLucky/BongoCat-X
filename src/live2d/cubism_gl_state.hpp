#pragma once
#include <GL/glew.h>
#include <array>
namespace bongo_cat {
struct GLState {
    bool active = false, program_valid = false, unit_valid = false, blend_valid = false;
    GLuint program = 0;
    GLenum unit = GL_TEXTURE0;
    std::array<GLuint, 8> textures{};
    std::array<bool, 8> texture_valid{};
    std::array<GLenum, 4> blend{};
};
inline thread_local GLState gl_state;
class GLStateScope {
public:
    GLStateScope() : previous_(gl_state) {
        gl_state = {}; gl_state.active = true;
        GLint unit = GL_TEXTURE0; glGetIntegerv(GL_ACTIVE_TEXTURE, &unit);
        gl_state.unit = (GLenum)unit; gl_state.unit_valid = true;
    }
    ~GLStateScope() { gl_state = {}; gl_state.active = previous_.active; }
private:
    GLState previous_;
};
inline void gl_use_program(GLuint program) {
    if (gl_state.active && gl_state.program_valid && gl_state.program == program) return;
    glUseProgram(program); gl_state.program = program; gl_state.program_valid = true;
}
inline void gl_active_texture(GLenum unit) {
    if (gl_state.active && gl_state.unit_valid && gl_state.unit == unit) return;
    glActiveTexture(unit); gl_state.unit = unit; gl_state.unit_valid = true;
}
inline void gl_bind_texture(GLenum target, GLuint texture) {
    const auto slot = (size_t)(gl_state.unit - GL_TEXTURE0);
    const bool cacheable = gl_state.active && gl_state.unit_valid && target == GL_TEXTURE_2D &&
        slot < gl_state.textures.size();
    if (cacheable && gl_state.texture_valid[slot] && gl_state.textures[slot] == texture) return;
    glBindTexture(target, texture);
    if (cacheable) { gl_state.textures[slot] = texture; gl_state.texture_valid[slot] = true; }
}
inline void gl_delete_textures(GLsizei count, const GLuint *textures) {
    for (GLsizei i = 0; i < count; ++i)
        for (size_t slot = 0; slot < gl_state.textures.size(); ++slot)
            if (gl_state.texture_valid[slot] && gl_state.textures[slot] == textures[i])
                gl_state.texture_valid[slot] = false;
    glDeleteTextures(count, textures);
}
inline void gl_delete_program(GLuint program) {
    if (gl_state.program == program) gl_state.program_valid = false;
    glDeleteProgram(program);
}
inline void gl_blend_func(GLenum sr, GLenum dr, GLenum sa, GLenum da) {
    const std::array<GLenum, 4> blend{sr, dr, sa, da};
    if (gl_state.active && gl_state.blend_valid && gl_state.blend == blend) return;
    glBlendFuncSeparate(sr, dr, sa, da); gl_state.blend = blend; gl_state.blend_valid = true;
}
}
