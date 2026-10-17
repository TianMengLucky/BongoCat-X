#include "image_bc7.h"
#include "image_internal.h"
#include "bongo_cat/gl_api.h"
#include "bongo_cat/log.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

bool bongo_cat_image_bc7_supported(void) {
#if (defined(_WIN32) || defined(__linux__)) && (defined(_M_X64) || defined(__x86_64__))
    const char *policy = SDL_getenv("BONGOCAT_OPENGL_BC7");
    if (policy && strcmp(policy, "0") == 0) return false;
    int major = 0, minor = 0;
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor);
    return SDL_GL_GetCurrentContext() && ((major > 4 || (major == 4 && minor >= 2)) ||
        SDL_GL_ExtensionSupported("GL_ARB_texture_compression_bptc")) &&
        SDL_GL_GetProcAddress("glCompressedTexImage2D") && SDL_GL_GetProcAddress("glBindBuffer");
#else
    return false;
#endif
}

unsigned int bongo_cat_image_upload_bc7(const BongoSafeNativeTexture *image, BongoCatError *error) {
    if (!image || !image->pixels || image->format != 1 || !image->width || !image->height ||
        image->width > 16384 || image->height > 16384 || image->size > INT_MAX ||
        image->size != ((size_t)image->width + 3) / 4 * (((size_t)image->height + 3) / 4) * 16 ||
        !bongo_cat_image_bc7_supported() || !bongo_cat_gl_clear_errors()) return 0;
    PFNGLCOMPRESSEDTEXIMAGE2DPROC upload = (PFNGLCOMPRESSEDTEXIMAGE2DPROC)
        SDL_GL_GetProcAddress("glCompressedTexImage2D");
    PFNGLBINDBUFFERPROC bind_buffer = (PFNGLBINDBUFFERPROC)SDL_GL_GetProcAddress("glBindBuffer");
    GLint previous = 0, unpack = 0;
    const GLenum parameters[] = {GL_UNPACK_ALIGNMENT, GL_UNPACK_ROW_LENGTH,
        GL_UNPACK_SKIP_PIXELS, GL_UNPACK_SKIP_ROWS};
    GLint values[4];
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
    glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpack);
    for (size_t i = 0; i < 4; ++i) {
        glGetIntegerv(parameters[i], &values[i]);
        glPixelStorei(parameters[i], i == 0 ? 1 : 0);
    }
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    bind_buffer(GL_PIXEL_UNPACK_BUFFER, 0);
    upload(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_BPTC_UNORM, (GLsizei)image->width,
        (GLsizei)image->height, 0, (GLsizei)image->size, image->pixels);
    bool ok = texture && glGetError() == GL_NO_ERROR;
    if (ok) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        ok = glGetError() == GL_NO_ERROR && bongo_cat_image_generate_mipmaps();
    }
    if (!ok) {
        glDeleteTextures(1, &texture); texture = 0;
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM, "OpenGL BC7 upload or mipmap generation failed");
        bongo_cat_gl_clear_errors();
    }
    glBindTexture(GL_TEXTURE_2D, (GLuint)previous);
    bind_buffer(GL_PIXEL_UNPACK_BUFFER, (GLuint)unpack);
    for (size_t i = 0; i < 4; ++i) glPixelStorei(parameters[i], values[i]);
    return texture;
}

typedef struct Bc7Load {
    const char *path;
    int width, height;
    BongoSafeNativeTexture image;
    bool success;
} Bc7Load;

static int SDLCALL decode_bc7(void *userdata) {
    Bc7Load *job = userdata;
    job->success = bongo_safe_native_texture_load(job->path, (uint32_t)job->width,
        (uint32_t)job->height, true, &job->image, NULL, NULL);
    return job->success ? 0 : -1;
}

unsigned int bongo_cat_image_texture_model_bc7(const char *path, int max_width,
    int max_height, int *width, int *height, BongoCatImageAlphaMask *alpha,
    BongoCatImageProgress progress, void *userdata, BongoCatError *error) {
    if (!path || max_width <= 0 || max_height <= 0 || !bongo_cat_image_bc7_supported()) return 0;
    Bc7Load *job = calloc(1, sizeof(*job));
    if (!job) return 0;
    job->path = path; job->width = max_width; job->height = max_height;
    SDL_Thread *thread = progress ? SDL_CreateThread(decode_bc7, BONGO_CAT_SLUG "-bc7-load", job) : NULL;
    if (thread) {
        while (SDL_GetThreadState(thread) == SDL_THREAD_ALIVE) {
            progress(userdata, 0.0f);
            SDL_Delay(2);
        }
        SDL_WaitThread(thread, NULL);
    } else decode_bc7(job);
    GLuint texture = job->success ? bongo_cat_image_upload_bc7(&job->image, error) : 0;
    if (texture) {
        if (width) *width = (int)job->image.width;
        if (height) *height = (int)job->image.height;
        if (alpha) {
            memcpy(alpha->pixels, job->image.alpha, sizeof(alpha->pixels));
            alpha->width = (int)SDL_min(job->image.source_width, 128u);
            alpha->height = (int)SDL_min(job->image.source_height, 128u);
        }
        SDL_LogInfo(BONGO_CAT_LOG_LIFECYCLE, "[opengl-atlas] format=bc7 size=%ux%u base_bytes=%zu mipmaps=1",
            job->image.width, job->image.height, job->image.size);
    }
    bongo_safe_free_pixels(job->image.pixels, job->image.size);
    free(job);
    return texture;
}
