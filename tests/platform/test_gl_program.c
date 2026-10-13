/* Exercise actual GL program ownership with failing dispatch, without a GPU. */
#include "bongo_cat/gl_api.h"
#include "test.h"
#include <string.h>
int bongo_cat_test_failures;
static unsigned live_shaders, live_programs, attached;
static bool fail_vertex, fail_fragment, fail_program, fail_link;
static GLuint APIENTRY shader(GLenum type) {
    live_shaders++;
    return type == GL_VERTEX_SHADER ? 1u : 2u;
}
static void APIENTRY source(GLuint id, GLsizei count, const GLchar *const *text, const GLint *length) {
    (void)id; (void)count; (void)text; (void)length;
}
static void APIENTRY compile(GLuint id) { (void)id; }
static void APIENTRY shader_status(GLuint id, GLenum key, GLint *status) {
    (void)key; *status = !((id == 1 && fail_vertex) || (id == 2 && fail_fragment));
}
static void APIENTRY message(GLuint id, GLsizei size, GLsizei *length, GLchar *text) {
    (void)id; (void)length;
    if (size > 0) text[0] = '\0';
}
static void APIENTRY delete_shader(GLuint id) {
    (void)id; CHECK(!attached); CHECK(live_shaders > 0); live_shaders--;
}
static GLuint APIENTRY program(void) {
    if (fail_program) return 0;
    live_programs++; return 3;
}
static void APIENTRY attach(GLuint p, GLuint id) { (void)p; (void)id; attached++; }
static void APIENTRY detach(GLuint p, GLuint id) {
    (void)p; (void)id; CHECK(attached > 0); attached--;
}
static void APIENTRY link(GLuint p) { (void)p; }
static void APIENTRY program_status(GLuint p, GLenum key, GLint *status) {
    (void)p; (void)key; *status = !fail_link;
}
static void APIENTRY delete_program(GLuint p) {
    (void)p; CHECK(!attached && live_programs > 0); live_programs--;
}
int main(void) {
    BongoCatGL gl = {0};
    gl.create_shader = shader; gl.shader_source = source; gl.compile_shader = compile;
    gl.get_shader_iv = shader_status; gl.get_shader_log = message;
    gl.delete_shader = delete_shader; gl.create_program = program;
    gl.attach_shader = attach; gl.detach_shader = detach; gl.link_program = link;
    gl.get_program_iv = program_status; gl.get_program_log = message;
    gl.delete_program = delete_program;
    for (unsigned mode = 0; mode < 5; ++mode) {
        fail_vertex = mode == 1; fail_fragment = mode == 2;
        fail_program = mode == 3; fail_link = mode == 4;
        BongoCatError error = {0};
        GLuint result = bongo_cat_gl_program(&gl, "vertex", "fragment", &error);
        CHECK(!live_shaders && !attached);
        if (!mode) {
            CHECK(result == 3 && live_programs == 1); delete_program(result);
        } else CHECK(!result && !live_programs && error.code != BONGO_CAT_OK);
    }
    return bongo_cat_test_failures ? 1 : 0;
}
