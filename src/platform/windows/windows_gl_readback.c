#include "windows_gl_readback.h"
#include "bongo_cat/rhi.h"
#include <SDL3/SDL_opengl.h>

bool bongo_cat_windows_gl_readback(int width, int height, void *pixels) {
    if (width <= 0 || height <= 0 || !pixels)
        return SDL_SetError("Invalid Windows frame readback dimensions");
    const BongoCatRhiPresentOps *ops = bongo_cat_rhi_active_present_ops();
    if (ops && ops->read_bgra)
        return ops->read_bgra(width, height, pixels, ops->user);
    PFNGLBINDBUFFERPROC bind_buffer =
        (PFNGLBINDBUFFERPROC)SDL_GL_GetProcAddress("glBindBuffer");
    PFNGLBINDFRAMEBUFFERPROC bind_framebuffer =
        (PFNGLBINDFRAMEBUFFERPROC)SDL_GL_GetProcAddress("glBindFramebuffer");
    if (!bind_buffer || !bind_framebuffer)
        return SDL_SetError("Windows frame readback requires OpenGL buffer bindings");
    /* Saved-state names avoid shadowing the window-frame globals. */
    GLint saved_framebuffer, saved_read_buffer, saved_pack_buffer, saved_alignment,
        saved_row_length, saved_skip_pixels, saved_skip_rows;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &saved_framebuffer);
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &saved_pack_buffer);
    glGetIntegerv(GL_PACK_ALIGNMENT, &saved_alignment);
    glGetIntegerv(GL_PACK_ROW_LENGTH, &saved_row_length);
    glGetIntegerv(GL_PACK_SKIP_PIXELS, &saved_skip_pixels);
    glGetIntegerv(GL_PACK_SKIP_ROWS, &saved_skip_rows);
    bind_framebuffer(GL_READ_FRAMEBUFFER, 0);
    /* GL_READ_BUFFER belongs to the bound framebuffer, so save/restore the
       default framebuffer's selector while that framebuffer is bound. */
    glGetIntegerv(GL_READ_BUFFER, &saved_read_buffer);
    bind_buffer(GL_PIXEL_PACK_BUFFER, 0);
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glReadPixels(0, 0, width, height, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
    GLenum gl_error = glGetError();
    glPixelStorei(GL_PACK_ALIGNMENT, saved_alignment);
    glPixelStorei(GL_PACK_ROW_LENGTH, saved_row_length);
    glPixelStorei(GL_PACK_SKIP_PIXELS, saved_skip_pixels);
    glPixelStorei(GL_PACK_SKIP_ROWS, saved_skip_rows);
    glReadBuffer((GLenum)saved_read_buffer);
    bind_buffer(GL_PIXEL_PACK_BUFFER, (GLuint)saved_pack_buffer);
    bind_framebuffer(GL_READ_FRAMEBUFFER, (GLuint)saved_framebuffer);
    return gl_error == GL_NO_ERROR ||
        SDL_SetError("Windows frame readback failed: 0x%x", (unsigned)gl_error);
}
