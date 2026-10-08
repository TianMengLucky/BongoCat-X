#include "cubism_plugin_services.hpp"
#include "bongo_cat/file.h"
#include "bongo_cat/model.h"
#include "bongo_cat/resource_trace.h"
#if defined(CSM_TARGET_WIN_GL) || defined(CSM_TARGET_LINUX_GL) || defined(CSM_TARGET_MAC_GL)
#include <GL/glew.h>
#endif
#include "cubism_runtime.hpp"
#include "cubism_render_resources.hpp"

#include <CubismFramework.hpp>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_video.h>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <exception>

#ifdef _WIN32
#include <malloc.h>
#endif
using Csm::CubismFramework;

BongoCatRhiBackend s_live2d_rhi_backend = BONGO_CAT_RHI_OPENGL;
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
namespace bongo_cat { void release_vulkan_device(); }
#endif
#ifdef BONGO_CAT_HAS_CUBISM_METAL
namespace bongo_cat { void release_metal_device(void *device); }
#endif

unsigned char *bongo_cat_cubism_shader_bytes(const std::string& path, size_t *size);

namespace {
class Allocator final : public Csm::ICubismAllocator {
public:
    void *Allocate(const Csm::csmSizeType size) override { return std::malloc(size); }
    void Deallocate(void *memory) override { std::free(memory); }
    void *AllocateAligned(const Csm::csmSizeType size, const Csm::csmUint32 alignment) override {
#ifdef _WIN32
        return _aligned_malloc(size, alignment);
#else
        void *memory = nullptr;
        return posix_memalign(&memory, alignment, size) == 0 ? memory : nullptr;
#endif
    }
    void DeallocateAligned(void *memory) override {
#ifdef _WIN32
        _aligned_free(memory);
#else
        std::free(memory);
#endif
    }
};

Allocator allocator;
CubismFramework::Option framework_option;
std::string resource_root;
int runtime_count;

void log_message(const char *message) {
    if (message) std::fprintf(stderr, "[Cubism] %s\n", message);
}

Csm::csmByte *load_file(const std::string path, Csm::csmSizeInt *size) {
    if (size) *size = 0;
    size_t shader_size = 0;
    if (auto *shader = bongo_cat_cubism_shader_bytes(path, &shader_size)) {
        if (size) *size = static_cast<Csm::csmSizeInt>(shader_size);
        return shader;
    }
    FILE *file = bongo_cat_file_open(path.c_str(), "rb");
    if (!file) {
        const char *base = SDL_GetBasePath();
        if (base) file = bongo_cat_file_open((std::string(base) + path).c_str(), "rb");
    }
    if (!file && !resource_root.empty())
        file = bongo_cat_file_open((resource_root + "/" + path).c_str(), "rb");
    if (!file) return nullptr;
    std::fseek(file, 0, SEEK_END);
    long length = std::ftell(file);
    std::rewind(file);
    if (length <= 0) { std::fclose(file); return nullptr; }
    auto *bytes = static_cast<Csm::csmByte *>(std::malloc((size_t)length));
    if (!bytes || std::fread(bytes, 1, (size_t)length, file) != (size_t)length) {
        std::free(bytes);
        bytes = nullptr;
    } else if (size) *size = (Csm::csmSizeInt)length;
    std::fclose(file);
    return bytes;
}

void release_file(Csm::csmByte *bytes) { std::free(bytes); }

bool start_framework(BongoCatError *error) {
    if (runtime_count++) return true;
#if defined(CSM_TARGET_WIN_GL) || defined(CSM_TARGET_LINUX_GL) || defined(CSM_TARGET_MAC_GL)
    if (bongo_cat_rhi_active_is_gl()) {
    glewExperimental = GL_TRUE;
    GLenum glew_result = glewInit();
    glGetError();
    if (glew_result != GLEW_OK) {
        runtime_count = 0;
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM, "GLEW initialization failed: %s",
            reinterpret_cast<const char *>(glewGetErrorString(glew_result)));
        return false;
    }
    if (!glCreateShader || !glShaderSource || !glCompileShader ||
        !glGetShaderiv || !glCreateProgram || !glGenFramebuffers ||
        !glGenBuffers || !glBindBuffer || !glBufferData || !glDeleteBuffers ||
        !glGenVertexArrays || !glBindVertexArray || !glDeleteVertexArrays ||
        !glEnableVertexAttribArray || !glVertexAttribPointer) {
        runtime_count = 0;
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "Required OpenGL 3.3 functions are unavailable");
        return false;
    }
    }
#endif
    framework_option = CubismFramework::Option{};
    framework_option.LogFunction = log_message;
    framework_option.LoggingLevel = CubismFramework::Option::LogLevel_Warning;
    framework_option.LoadFileFunction = load_file;
    framework_option.ReleaseBytesFunction = release_file;
    if (!CubismFramework::StartUp(&allocator, &framework_option)) {
        runtime_count = 0;
        bongo_cat_error_set(error, BONGO_CAT_ERROR_CUBISM, "Cubism Framework startup failed");
        return false;
    }
    CubismFramework::Initialize();
    return true;
}

void stop_framework() {
    if (--runtime_count > 0) return;
    CubismFramework::Dispose();
    CubismFramework::CleanUp();
    // Core outlives the plugin; it must not retain the unloaded log callback.
    Live2D::Cubism::Core::csmSetLogFunction(nullptr);
    runtime_count = 0;
}

} // namespace

static BongoCatModelRuntime *create_runtime(const char *asset_root,
    BongoCatError *error) {
    resource_root = asset_root ? asset_root : "";
    BongoCatRhiDeviceInfo device{};
    bongo_cat_rhi_get_active_device_info(&device);
    s_live2d_rhi_backend = device.backend;
    if (!start_framework(error)) return nullptr;
    BongoCatModelRuntime *runtime = new(std::nothrow) BongoCatModelRuntime{};
    if (!runtime) {
        stop_framework();
        bongo_cat_error_set(error, BONGO_CAT_ERROR_MEMORY, "Cannot allocate Cubism runtime");
    }
    if (runtime) {
        bongo_cat_rhi_get_active_device_info(&runtime->rhi_info);
        s_live2d_rhi_backend = runtime->rhi_info.backend;

    }
    return runtime;
}

extern "C" BongoCatModelRuntime *bongo_cat_model_runtime_create(const char *asset_root,
    BongoCatError *error) {
    return create_runtime(asset_root, error);
}

extern "C" void bongo_cat_model_runtime_destroy(BongoCatModelRuntime *runtime) {
    if (!runtime) return;
    delete runtime->model;
    bongo_cat_resource_trace_atlas(0.0);
    /* Retire the renderer before its unused singleton targets, while the
       owning GL context is still available. */
    if (runtime_count == 1 && runtime->rhi_info.backend == BONGO_CAT_RHI_OPENGL)
        bongo_cat::release_offscreen_pool();
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
    if (runtime_count == 1 && runtime->rhi_info.backend == BONGO_CAT_RHI_VULKAN)
        bongo_cat::release_vulkan_device();
#endif
#ifdef BONGO_CAT_HAS_CUBISM_METAL
    if (runtime_count == 1 && runtime->rhi_info.backend == BONGO_CAT_RHI_METAL)
        bongo_cat::release_metal_device(runtime->rhi_info.metal_device);
#endif
    delete runtime;
    stop_framework();
}

extern "C" bool bongo_cat_model_runtime_ready(const BongoCatModelRuntime *runtime) {
    return runtime && runtime->model;
}

extern "C" bool bongo_cat_model_runtime_canvas_size(const BongoCatModelRuntime *runtime,
    int *width, int *height) {
    return runtime && runtime->model &&
        runtime->model->canvas_size(width, height);
}

extern "C" bool bongo_cat_model_runtime_frame(const BongoCatModelRuntime *runtime,
    BongoCatModelRuntimeFrame *frame) {
    return runtime && runtime->model && runtime->model->frame(frame);
}

extern "C" bool bongo_cat_model_runtime_viewport(const BongoCatModelRuntime *runtime,
    int *x, int *y, int *width, int *height) {
    return runtime && runtime->model &&
        runtime->model->viewport(x, y, width, height);
}

extern "C" bool bongo_cat_model_runtime_overlay_viewport(const BongoCatModelRuntime *runtime,
    int *x, int *y, int *width, int *height) {
    return runtime && runtime->model &&
        runtime->model->overlay_viewport(x, y, width, height);
}

extern "C" void bongo_cat_model_runtime_resize(BongoCatModelRuntime *runtime, int width, int height) {
    if (!runtime) return;
    if (width > 0 && height > 0) {
        runtime->width = width;
        runtime->height = height;
    }
    if (runtime->model) runtime->model->resize(width, height);
}
extern "C" void bongo_cat_model_runtime_reshape(BongoCatModelRuntime *runtime, int width, int height) {
    if (!runtime) return;
    if (width > 0 && height > 0) {
        runtime->width = width;
        runtime->height = height;
    }
    if (runtime->model) runtime->model->reshape(width, height);
}
extern "C" bool bongo_cat_model_runtime_update(BongoCatModelRuntime *runtime, float elapsed) {
    if (!runtime) return false;
    return runtime->model && runtime->model->update(elapsed);
}

extern "C" bool bongo_cat_model_runtime_texture_refresh_pending(const BongoCatModelRuntime *runtime, bool active) {
    return runtime && runtime->model && runtime->model->texture_refresh_pending(active);
}

extern "C" bool bongo_cat_model_runtime_texture_refresh_due(const BongoCatModelRuntime *runtime,
    bool active, bool allow_start) {
    return runtime && runtime->model && runtime->model->texture_refresh_due(active, allow_start);
}

extern "C" bool bongo_cat_model_runtime_try_reuse_texture_quality(BongoCatModelRuntime *runtime,
    float quality_percent) {
    return runtime && runtime->model && runtime->model->try_reuse_texture_quality(quality_percent);
}

extern "C" bool bongo_cat_model_runtime_measure_frame(BongoCatModelRuntime *runtime,
    BongoCatModelRuntimeFrame *required) {
    return runtime && runtime->model && runtime->model->measure_frame(required);
}

extern "C" void bongo_cat_model_runtime_set_frame(BongoCatModelRuntime *runtime,
    const BongoCatModelRuntimeFrame *frame) {
    if (runtime && runtime->model && frame) runtime->model->set_frame(*frame);
}
extern "C" bool bongo_cat_model_runtime_refresh_textures(BongoCatModelRuntime *runtime,
    bool active, bool allow_start) {
    return runtime && runtime->model && runtime->model->refresh_texture_resolution(active, allow_start);
}
extern "C" bool bongo_cat_model_runtime_texture_refresh_busy(const BongoCatModelRuntime *runtime) {
    return runtime && runtime->model && runtime->model->texture_refresh_busy();
}
extern "C" void bongo_cat_model_runtime_cancel_texture_refresh(BongoCatModelRuntime *runtime) {
    if (runtime && runtime->model) runtime->model->cancel_texture_refresh_async();
}
extern "C" bool bongo_cat_model_runtime_draw_checked(BongoCatModelRuntime *runtime) {
    if (!runtime || !runtime->model ||
        !runtime->model->GetRenderer<Csm::Rendering::CubismRenderer>()) return false;
    try { runtime->model->draw(); return true; }
    catch (const std::exception &exception) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Live2D draw failed: %s", exception.what());
    } catch (...) { SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Live2D draw failed"); }
    // A failed native draw may leave partially initialized SDK pipeline caches.
    // Retire them before a retry can bind an incomplete pipeline.
    if (runtime->rhi_info.backend != BONGO_CAT_RHI_OPENGL)
        runtime->model->release_render_resources();
    return false;
}
extern "C" void bongo_cat_model_runtime_draw(BongoCatModelRuntime *runtime) {
    (void)bongo_cat_model_runtime_draw_checked(runtime);
}
extern "C" void bongo_cat_model_runtime_set_vertical_flip(BongoCatModelRuntime *runtime, bool flipped) {
    if (runtime && runtime->model) runtime->model->set_vertical_flip(flipped);
}

extern "C" void bongo_cat_model_runtime_set_rhi_info(BongoCatModelRuntime *runtime,
    const BongoCatRhiDeviceInfo *info) {
    if (!runtime) return;
    runtime->rhi_info = info ? *info : BongoCatRhiDeviceInfo{};
    s_live2d_rhi_backend = runtime->rhi_info.backend;
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
    if (info && info->backend == BONGO_CAT_RHI_VULKAN && volkInitialize() == VK_SUCCESS) {
        volkLoadInstance((VkInstance)info->vulkan_instance);
        volkLoadDevice((VkDevice)info->vulkan_device);
    }
#endif
    if (runtime->model) runtime->model->set_rhi_info(runtime->rhi_info);
}

extern "C" void bongo_cat_model_runtime_set_mirror(BongoCatModelRuntime *runtime, bool mirror) {
    if (runtime && runtime->model) runtime->model->set_mirror(mirror); }
extern "C" void bongo_cat_model_runtime_set_render_options(BongoCatModelRuntime *runtime,
    const BongoCatModelRuntimeRenderOptions *options) {
    if (runtime && runtime->model && options)
        runtime->model->set_render_options(*options); }
extern "C" void bongo_cat_model_runtime_set_tight_frame(BongoCatModelRuntime *runtime, bool tight) {
    if (runtime && runtime->model) runtime->model->set_tight_frame(tight); }
extern "C" void bongo_cat_model_runtime_set_tight_overlay_rect(BongoCatModelRuntime *runtime,
    const float *rect) {
    if (runtime && runtime->model) runtime->model->set_tight_overlay_rect(rect); }
extern "C" void bongo_cat_model_runtime_set_dragging(BongoCatModelRuntime *runtime,
    float x, float y) {
    if (runtime && runtime->model) runtime->model->set_dragging(x, y); }
extern "C" void bongo_cat_model_runtime_set_centered_dragging(BongoCatModelRuntime *runtime,
    float x, float y) {
    if (runtime && runtime->model) runtime->model->set_dragging(x, y, true); }
extern "C" void bongo_cat_model_runtime_prepare_viewer_audit(BongoCatModelRuntime *runtime) {
    if (runtime && runtime->model) runtime->model->prepare_viewer_audit();
}
extern "C" bool bongo_cat_model_runtime_prepare_cover_capture(
    BongoCatModelRuntime *runtime) {
    return runtime && runtime->model &&
        runtime->model->prepare_cover_capture();
}
extern "C" bool bongo_cat_model_runtime_set_parameter(BongoCatModelRuntime *runtime, const char *id, float value) {
    return runtime && runtime->model && runtime->model->set_parameter(id, value);
}
extern "C" bool bongo_cat_model_runtime_parameter(BongoCatModelRuntime *runtime, const char *id,
    BongoCatParameterRange *range) {
    return runtime && runtime->model && range && runtime->model->parameter(id,
        &range->minimum, &range->maximum, &range->value);
}
extern "C" bool bongo_cat_model_runtime_start_motion(BongoCatModelRuntime *runtime, const char *group, int index) {
    return runtime && runtime->model && runtime->model->start_motion(group, index);
}
extern "C" bool bongo_cat_model_runtime_restore_motion_state(BongoCatModelRuntime *runtime,
    const char *group, int index) {
    return runtime && runtime->model &&
        runtime->model->restore_motion_state(group, index);
}
extern "C" bool bongo_cat_model_runtime_preview_motion(BongoCatModelRuntime *runtime, const char *group, int index) {
    return runtime && runtime->model && runtime->model->preview_motion(group, index); }
extern "C" bool bongo_cat_model_runtime_restore_motion_preview(BongoCatModelRuntime *runtime) { return runtime && runtime->model && runtime->model->restore_motion_preview(); }
extern "C" bool bongo_cat_model_runtime_commit_motion_preview(BongoCatModelRuntime *runtime, const char *group, int index) {
    return runtime && runtime->model && runtime->model->commit_motion_preview(group, index); }
extern "C" bool bongo_cat_model_runtime_motion_selected(const BongoCatModelRuntime *runtime, const char *group, int index) { return runtime && runtime->model && runtime->model->motion_selected(group, index); }
extern "C" bool bongo_cat_model_runtime_motion_persistent(const BongoCatModelRuntime *runtime,
    const char *group, int index) {
    return runtime && runtime->model &&
        runtime->model->motion_persistent(group, index); }
extern "C" bool bongo_cat_model_runtime_motion_visible(const BongoCatModelRuntime *runtime, const char *group, int index) {
    return runtime && runtime->model && runtime->model->motion_visible(group, index); }
extern "C" bool bongo_cat_model_runtime_motion_same_toggle(
    const BongoCatModelRuntime *runtime, const char *left_group, int left_index,
    const char *right_group, int right_index) {
    return runtime && runtime->model && runtime->model->motion_same_toggle(
        left_group, left_index, right_group, right_index); }
extern "C" bool bongo_cat_model_runtime_set_expression(BongoCatModelRuntime *runtime, int index) {
    return runtime && runtime->model && runtime->model->set_expression(index); }
extern "C" int bongo_cat_model_runtime_expression(const BongoCatModelRuntime *runtime) {
    return runtime && runtime->model ? runtime->model->expression() : -1; }
