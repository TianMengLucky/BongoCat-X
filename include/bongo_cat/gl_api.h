#ifndef BONGO_CAT_GL_API_H
#define BONGO_CAT_GL_API_H

#include "bongo_cat/common.h"
#include <SDL3/SDL_opengl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BongoCatGL {
    PFNGLCREATESHADERPROC create_shader;
    PFNGLSHADERSOURCEPROC shader_source;
    PFNGLCOMPILESHADERPROC compile_shader;
    PFNGLGETSHADERIVPROC get_shader_iv;
    PFNGLGETSHADERINFOLOGPROC get_shader_log;
    PFNGLCREATEPROGRAMPROC create_program;
    PFNGLATTACHSHADERPROC attach_shader;
    PFNGLDETACHSHADERPROC detach_shader;
    PFNGLLINKPROGRAMPROC link_program;
    PFNGLGETPROGRAMIVPROC get_program_iv;
    PFNGLGETPROGRAMINFOLOGPROC get_program_log;
    PFNGLDELETESHADERPROC delete_shader;
    PFNGLDELETEPROGRAMPROC delete_program;
    PFNGLUSEPROGRAMPROC use_program;
    PFNGLGENVERTEXARRAYSPROC gen_vertex_arrays;
    PFNGLBINDVERTEXARRAYPROC bind_vertex_array;
    PFNGLDELETEVERTEXARRAYSPROC delete_vertex_arrays;
    PFNGLGENBUFFERSPROC gen_buffers;
    PFNGLBINDBUFFERPROC bind_buffer;
    PFNGLBUFFERDATAPROC buffer_data;
    PFNGLBUFFERSUBDATAPROC buffer_sub_data;
    PFNGLDELETEBUFFERSPROC delete_buffers;
    PFNGLENABLEVERTEXATTRIBARRAYPROC enable_attribute;
    PFNGLVERTEXATTRIBPOINTERPROC attribute_pointer;
    PFNGLGETUNIFORMLOCATIONPROC uniform_location;
    PFNGLUNIFORM1IPROC uniform_1i;
    PFNGLUNIFORMMATRIX4FVPROC uniform_matrix_4fv;
    PFNGLACTIVETEXTUREPROC active_texture;
    PFNGLBLENDEQUATIONPROC blend_equation;
    PFNGLBLENDFUNCSEPARATEPROC blend_func_separate;
} BongoCatGL;

bool bongo_cat_gl_load(BongoCatGL *gl, BongoCatError *error);
bool bongo_cat_gl_clear_errors(void);
unsigned int bongo_cat_gl_program(BongoCatGL *gl, const char *vertex, const char *fragment,
    BongoCatError *error);

#ifdef __cplusplus
}
#endif

#endif
