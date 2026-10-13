#include "bongo_cat/gl_api.h"

#include <SDL3/SDL.h>
#include <string.h>

#define LOAD(member, type, name) do { \
    gl->member = (type)SDL_GL_GetProcAddress(name); \
    if (!gl->member) { bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM, \
        "Missing OpenGL function: %s", name); return false; } \
} while (0)

bool bongo_cat_gl_load(BongoCatGL *gl, BongoCatError *error) {
    if (!gl) return false;
    memset(gl, 0, sizeof(*gl));
    LOAD(create_shader, PFNGLCREATESHADERPROC, "glCreateShader");
    LOAD(shader_source, PFNGLSHADERSOURCEPROC, "glShaderSource");
    LOAD(compile_shader, PFNGLCOMPILESHADERPROC, "glCompileShader");
    LOAD(get_shader_iv, PFNGLGETSHADERIVPROC, "glGetShaderiv");
    LOAD(get_shader_log, PFNGLGETSHADERINFOLOGPROC, "glGetShaderInfoLog");
    LOAD(create_program, PFNGLCREATEPROGRAMPROC, "glCreateProgram");
    LOAD(attach_shader, PFNGLATTACHSHADERPROC, "glAttachShader");
    LOAD(detach_shader, PFNGLDETACHSHADERPROC, "glDetachShader");
    LOAD(link_program, PFNGLLINKPROGRAMPROC, "glLinkProgram");
    LOAD(get_program_iv, PFNGLGETPROGRAMIVPROC, "glGetProgramiv");
    LOAD(get_program_log, PFNGLGETPROGRAMINFOLOGPROC, "glGetProgramInfoLog");
    LOAD(delete_shader, PFNGLDELETESHADERPROC, "glDeleteShader");
    LOAD(delete_program, PFNGLDELETEPROGRAMPROC, "glDeleteProgram");
    LOAD(use_program, PFNGLUSEPROGRAMPROC, "glUseProgram");
    LOAD(gen_vertex_arrays, PFNGLGENVERTEXARRAYSPROC, "glGenVertexArrays");
    LOAD(bind_vertex_array, PFNGLBINDVERTEXARRAYPROC, "glBindVertexArray");
    LOAD(delete_vertex_arrays, PFNGLDELETEVERTEXARRAYSPROC, "glDeleteVertexArrays");
    LOAD(gen_buffers, PFNGLGENBUFFERSPROC, "glGenBuffers");
    LOAD(bind_buffer, PFNGLBINDBUFFERPROC, "glBindBuffer");
    LOAD(buffer_data, PFNGLBUFFERDATAPROC, "glBufferData");
    LOAD(buffer_sub_data, PFNGLBUFFERSUBDATAPROC, "glBufferSubData");
    LOAD(delete_buffers, PFNGLDELETEBUFFERSPROC, "glDeleteBuffers");
    LOAD(enable_attribute, PFNGLENABLEVERTEXATTRIBARRAYPROC, "glEnableVertexAttribArray");
    LOAD(attribute_pointer, PFNGLVERTEXATTRIBPOINTERPROC, "glVertexAttribPointer");
    LOAD(uniform_location, PFNGLGETUNIFORMLOCATIONPROC, "glGetUniformLocation");
    LOAD(uniform_1i, PFNGLUNIFORM1IPROC, "glUniform1i");
    LOAD(uniform_matrix_4fv, PFNGLUNIFORMMATRIX4FVPROC, "glUniformMatrix4fv");
    LOAD(active_texture, PFNGLACTIVETEXTUREPROC, "glActiveTexture");
    LOAD(blend_equation, PFNGLBLENDEQUATIONPROC, "glBlendEquation");
    LOAD(blend_func_separate, PFNGLBLENDFUNCSEPARATEPROC, "glBlendFuncSeparate");
    return true;
}

bool bongo_cat_gl_clear_errors(void) {
    for (unsigned attempt = 0; attempt < 16; ++attempt)
        if (glGetError() == GL_NO_ERROR) return true;
    return false;
}

static GLuint compile(BongoCatGL *gl, GLenum type, const char *source, BongoCatError *error) {
    GLuint shader = gl->create_shader(type);
    gl->shader_source(shader, 1, &source, NULL);
    gl->compile_shader(shader);
    GLint ok = GL_FALSE;
    gl->get_shader_iv(shader, GL_COMPILE_STATUS, &ok);
    if (ok) return shader;
    char message[512];
    gl->get_shader_log(shader, sizeof(message), NULL, message);
    bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM, "Shader compilation failed: %s", message);
    gl->delete_shader(shader);
    return 0;
}

unsigned int bongo_cat_gl_program(BongoCatGL *gl, const char *vertex, const char *fragment,
    BongoCatError *error) {
    GLuint vs = compile(gl, GL_VERTEX_SHADER, vertex, error);
    GLuint fs = compile(gl, GL_FRAGMENT_SHADER, fragment, error);
    if (!vs || !fs) {
        if (vs) gl->delete_shader(vs);
        if (fs) gl->delete_shader(fs);
        return 0;
    }
    GLuint program = gl->create_program();
    if (!program) {
        gl->delete_shader(vs); gl->delete_shader(fs);
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM, "Shader program allocation failed");
        return 0;
    }
    gl->attach_shader(program, vs);
    gl->attach_shader(program, fs);
    gl->link_program(program);
    gl->detach_shader(program, vs);
    gl->detach_shader(program, fs);
    gl->delete_shader(vs);
    gl->delete_shader(fs);
    GLint ok = GL_FALSE;
    gl->get_program_iv(program, GL_LINK_STATUS, &ok);
    if (ok) return program;
    char message[512];
    gl->get_program_log(program, sizeof(message), NULL, message);
    bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM, "Shader link failed: %s", message);
    gl->delete_program(program);
    return 0;
}
