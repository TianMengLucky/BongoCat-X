// Windows-only C++ bridge; the host and public presenter interface remain C11.
#include "windows_composition.h"
#include "bongo_cat/log.h"
#include <SDL3/SDL.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <wrl/client.h>
#include <SDL3/SDL_opengl.h>
#include <limits>
#include <new>

using Microsoft::WRL::ComPtr;
namespace {
struct Presenter {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain1> swap;
    ComPtr<IDCompositionDevice> composition;
    ComPtr<IDCompositionTarget> target;
    ComPtr<IDCompositionVisual> visual;
    ComPtr<IDCompositionEffectGroup> effect;
    using OpenDevice = HANDLE (WINAPI *)(void *);
    using CloseDevice = BOOL (WINAPI *)(HANDLE);
    using RegisterObject = HANDLE (WINAPI *)(HANDLE, void *, GLuint, GLenum, GLenum);
    using UnregisterObject = BOOL (WINAPI *)(HANDLE, HANDLE);
    using LockObjects = BOOL (WINAPI *)(HANDLE, GLint, HANDLE *);
    OpenDevice open_device = nullptr;
    CloseDevice close_device = nullptr;
    RegisterObject register_object = nullptr;
    UnregisterObject unregister_object = nullptr;
    LockObjects lock_objects = nullptr, unlock_objects = nullptr;
    PFNGLGENFRAMEBUFFERSPROC gen_framebuffers = nullptr;
    PFNGLDELETEFRAMEBUFFERSPROC delete_framebuffers = nullptr;
    PFNGLBINDFRAMEBUFFERPROC bind_framebuffer = nullptr;
    PFNGLGENRENDERBUFFERSPROC gen_renderbuffers = nullptr;
    PFNGLDELETERENDERBUFFERSPROC delete_renderbuffers = nullptr;
    PFNGLBINDRENDERBUFFERPROC bind_renderbuffer = nullptr;
    PFNGLRENDERBUFFERSTORAGEPROC renderbuffer_storage = nullptr;
    PFNGLFRAMEBUFFERRENDERBUFFERPROC attach = nullptr;
    PFNGLCHECKFRAMEBUFFERSTATUSPROC check_framebuffer = nullptr;
    PFNGLBLITFRAMEBUFFERPROC blit = nullptr;
    ComPtr<ID3D11Texture2D> shared_texture;
    HANDLE interop = nullptr, object = nullptr;
    GLuint framebuffer = 0, color = 0, resolve_framebuffer = 0, resolve_color = 0;
    int resolve_width = 0, resolve_height = 0;
    SDL_Window *gl_window = nullptr;
    SDL_GLContext gl_context = nullptr;
    int width = 0, height = 0;
    float opacity = -1.0f;
    bool committed = false;
};

HRESULT initialize(Presenter &p, HWND window, int width, int height) {
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
        p.device.GetAddressOf(), nullptr, p.context.GetAddressOf());
    if (FAILED(hr)) return hr;
    ComPtr<IDXGIDevice> dxgi;
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<IDXGIFactory2> factory;
    if (FAILED(hr = p.device.As(&dxgi)) ||
        FAILED(hr = dxgi->GetAdapter(adapter.GetAddressOf())) ||
        FAILED(hr = adapter->GetParent(IID_PPV_ARGS(factory.GetAddressOf())))) return hr;
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = static_cast<UINT>(width);
    desc.Height = static_cast<UINT>(height);
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    if (FAILED(hr = factory->CreateSwapChainForComposition(p.device.Get(), &desc,
            nullptr, p.swap.GetAddressOf())) ||
        FAILED(hr = DCompositionCreateDevice(dxgi.Get(),
            IID_PPV_ARGS(p.composition.GetAddressOf()))) ||
        FAILED(hr = p.composition->CreateTargetForHwnd(window, TRUE, p.target.GetAddressOf())) ||
        FAILED(hr = p.composition->CreateVisual(p.visual.GetAddressOf())) ||
        FAILED(hr = p.composition->CreateEffectGroup(p.effect.GetAddressOf())) ||
        FAILED(hr = p.visual->SetEffect(p.effect.Get())) ||
        FAILED(hr = p.visual->SetContent(p.swap.Get())) ||
        FAILED(hr = p.target->SetRoot(p.visual.Get()))) return hr;
    p.width = width;
    p.height = height;
    LONG_PTR style = GetWindowLongPtrW(window, GWL_EXSTYLE);
    SetLastError(ERROR_SUCCESS);
    if (!SetWindowLongPtrW(window, GWL_EXSTYLE,
            style | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOREDIRECTIONBITMAP) && GetLastError())
        return HRESULT_FROM_WIN32(GetLastError());
    // Uniform layered alpha provides cross-process click-through without an alpha bitmap.
    if (!SetLayeredWindowAttributes(window, 0, 255, LWA_ALPHA))
        return HRESULT_FROM_WIN32(GetLastError());
    if (!SetWindowPos(window, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE |
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED))
        return HRESULT_FROM_WIN32(GetLastError());
    return S_OK;
}

template<class T> bool load(T &proc, const char *name) {
    proc = reinterpret_cast<T>(SDL_GL_GetProcAddress(name));
    return proc != nullptr;
}

bool load_interop(Presenter &p) {
    p.gl_window = SDL_GL_GetCurrentWindow();
    p.gl_context = SDL_GL_GetCurrentContext();
    if (!p.gl_window || !p.gl_context)
        return SDL_SetError("DirectComposition requires a current OpenGL context");
    using ExtensionString = const char *(WINAPI *)(HDC);
    using CurrentDC = HDC (WINAPI *)();
    ExtensionString extensions = nullptr;
    CurrentDC current_dc = nullptr;
    if (!load(extensions, "wglGetExtensionsStringARB") || !load(current_dc, "wglGetCurrentDC"))
        return SDL_SetError("Cannot query Windows OpenGL GPU interop support");
    const char *supported = extensions(current_dc());
    if (!supported || !SDL_strstr(supported, "WGL_NV_DX_interop2"))
        return SDL_SetError("Driver does not advertise OpenGL/D3D11 GPU interop");
    bool loaded = load(p.open_device, "wglDXOpenDeviceNV") &&
        load(p.close_device, "wglDXCloseDeviceNV") &&
        load(p.register_object, "wglDXRegisterObjectNV") &&
        load(p.unregister_object, "wglDXUnregisterObjectNV") &&
        load(p.lock_objects, "wglDXLockObjectsNV") &&
        load(p.unlock_objects, "wglDXUnlockObjectsNV") &&
        load(p.gen_framebuffers, "glGenFramebuffers") &&
        load(p.delete_framebuffers, "glDeleteFramebuffers") &&
        load(p.bind_framebuffer, "glBindFramebuffer") &&
        load(p.gen_renderbuffers, "glGenRenderbuffers") &&
        load(p.delete_renderbuffers, "glDeleteRenderbuffers") &&
        load(p.bind_renderbuffer, "glBindRenderbuffer") &&
        load(p.renderbuffer_storage, "glRenderbufferStorage") &&
        load(p.attach, "glFramebufferRenderbuffer") &&
        load(p.check_framebuffer, "glCheckFramebufferStatus") &&
        load(p.blit, "glBlitFramebuffer");
    return loaded || SDL_SetError("Driver does not support OpenGL/D3D11 GPU interop");
}

bool release_texture(Presenter &p) {
    if (p.object && !p.unregister_object(p.interop, p.object))
        return SDL_SetError("Cannot unregister DirectComposition GPU texture");
    p.object = nullptr;
    if (p.color) p.delete_renderbuffers(1, &p.color);
    if (p.framebuffer) p.delete_framebuffers(1, &p.framebuffer);
    p.color = p.framebuffer = 0;
    p.shared_texture.Reset();
    return true;
}

bool share_texture(Presenter &p) {
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = static_cast<UINT>(p.width);
    desc.Height = static_cast<UINT>(p.height);
    desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    HRESULT hr = p.device->CreateTexture2D(&desc, nullptr, p.shared_texture.GetAddressOf());
    if (FAILED(hr)) return SDL_SetError("Cannot create DirectComposition GPU texture (0x%08lx)",
        static_cast<unsigned long>(hr));
    p.gen_renderbuffers(1, &p.color);
    constexpr GLenum interop_write_discard = 0x0002; // WGL_ACCESS_WRITE_DISCARD_NV
    p.object = p.register_object(p.interop, p.shared_texture.Get(), p.color,
        GL_RENDERBUFFER, interop_write_discard);
    if (!p.object) return SDL_SetError("Cannot share DirectComposition GPU texture (%lu)", GetLastError());
    p.gen_framebuffers(1, &p.framebuffer);
    return true;
}

bool copy_frame(Presenter &p, int width, int height) {
    if (!p.lock_objects(p.interop, 1, &p.object))
        return SDL_SetError("Cannot lock DirectComposition GPU texture");
    GLint read_fbo, draw_fbo, renderbuffer, read_buffer;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_fbo);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fbo);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer);
    GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean srgb = glIsEnabled(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_FRAMEBUFFER_SRGB);
    p.bind_framebuffer(GL_READ_FRAMEBUFFER, 0);
    glGetIntegerv(GL_READ_BUFFER, &read_buffer);
    glReadBuffer(GL_BACK);
    if (!p.resolve_framebuffer) {
        p.gen_framebuffers(1, &p.resolve_framebuffer);
        p.gen_renderbuffers(1, &p.resolve_color);
    }
    p.bind_renderbuffer(GL_RENDERBUFFER, p.resolve_color);
    if (p.resolve_width != width || p.resolve_height != height) {
        p.renderbuffer_storage(GL_RENDERBUFFER, GL_RGBA8, width, height);
        p.resolve_width = width; p.resolve_height = height;
    }
    p.bind_framebuffer(GL_DRAW_FRAMEBUFFER, p.resolve_framebuffer);
    p.attach(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, p.resolve_color);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    bool complete = p.check_framebuffer(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    // Resolve MSAA before the second blit flips and scales the image for DXGI.
    if (complete) p.blit(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    p.bind_framebuffer(GL_READ_FRAMEBUFFER, p.resolve_framebuffer);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    p.bind_framebuffer(GL_DRAW_FRAMEBUFFER, p.framebuffer);
    p.attach(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, p.color);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    complete = complete && p.check_framebuffer(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    if (complete) p.blit(0, 0, width, height, 0, p.height, p.width, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    GLenum error = glGetError();
    p.bind_framebuffer(GL_READ_FRAMEBUFFER, 0);
    glReadBuffer(static_cast<GLenum>(read_buffer));
    p.bind_framebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(read_fbo));
    p.bind_framebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(draw_fbo));
    p.bind_renderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(renderbuffer));
    if (scissor) glEnable(GL_SCISSOR_TEST);
    if (srgb) glEnable(GL_FRAMEBUFFER_SRGB);
    bool unlocked = p.unlock_objects(p.interop, 1, &p.object) != FALSE;
    return (complete && error == GL_NO_ERROR && unlocked) ||
        SDL_SetError("DirectComposition GPU copy failed (GL=0x%x, complete=%d, unlocked=%d)",
            error, complete, unlocked);
}

bool present(Presenter &p, HWND window, const RECT &bounds,
    int width, int height, float opacity) {
    int display_width = bounds.right - bounds.left, display_height = bounds.bottom - bounds.top;
    HRESULT hr;
    if (!p.device) {
        if (!load_interop(p)) return false;
        hr = initialize(p, window, display_width, display_height);
        if (FAILED(hr)) return SDL_SetError("DirectComposition initialization failed (0x%08lx)",
            static_cast<unsigned long>(hr));
        p.interop = p.open_device(p.device.Get());
        if (!p.interop) return SDL_SetError("Cannot open OpenGL/D3D11 interop device (%lu)", GetLastError());
    }
    if (p.width != display_width || p.height != display_height) {
        if (!release_texture(p)) return false;
        hr = p.swap->ResizeBuffers(0, static_cast<UINT>(display_width), static_cast<UINT>(display_height),
            DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(hr)) return SDL_SetError("DXGI resize failed (0x%08lx)", static_cast<unsigned long>(hr));
        p.width = display_width; p.height = display_height;
    }
    if ((!p.object && !share_texture(p)) || !copy_frame(p, width, height)) return false;
    RECT current;
    if (!GetWindowRect(window, &current)) return SDL_SetError("Cannot locate DXGI window");
    if (current.left != bounds.left || current.top != bounds.top ||
        current.right != bounds.right || current.bottom != bounds.bottom) {
        if (!SetWindowPos(window, nullptr, bounds.left, bounds.top, display_width, display_height,
                SWP_NOZORDER | SWP_NOACTIVATE)) return SDL_SetError("Cannot position DXGI window");
    }
    ComPtr<ID3D11Texture2D> buffer;
    hr = p.swap->GetBuffer(0, IID_PPV_ARGS(buffer.GetAddressOf()));
    if (FAILED(hr)) return SDL_SetError("DXGI back buffer failed (0x%08lx)", static_cast<unsigned long>(hr));
    p.context->CopyResource(buffer.Get(), p.shared_texture.Get());
    if (!p.committed || p.opacity != opacity) {
        hr = p.effect->SetOpacity(opacity);
        if (SUCCEEDED(hr)) hr = p.composition->Commit();
        if (FAILED(hr)) return SDL_SetError("DirectComposition commit failed (0x%08lx)", static_cast<unsigned long>(hr));
        p.opacity = opacity;
        p.committed = true;
    }
    hr = p.swap->Present(0, 0);
    if (FAILED(hr)) return SDL_SetError("DXGI present failed (0x%08lx)", static_cast<unsigned long>(hr));
    return true;
}
} // namespace

extern "C" bool bongo_cat_windows_composition_present(void **state, HWND window,
    const RECT *bounds, int width, int height, float opacity) {
    if (!state || !window || !bounds || width <= 0 || height <= 0 ||
        bounds->right <= bounds->left || bounds->bottom <= bounds->top ||
        width > (std::numeric_limits<int>::max)() / 4 ||
        static_cast<size_t>(height) > (std::numeric_limits<size_t>::max)() /
            (static_cast<size_t>(width) * 4)) return SDL_SetError("Invalid DXGI frame");
    if (SDL_GetHintBoolean("BONGO_CAT_TEST_COMPOSITION_FAILURE", false))
        return SDL_SetError("Injected DirectComposition failure");
    try {
        bool created = !*state;
        if (created) *state = new Presenter;
        bool result = present(*static_cast<Presenter *>(*state), window, *bounds,
            width, height, opacity);
        if (result && created) SDL_LogInfo(BONGO_CAT_LOG_LIFECYCLE,
            "Windows presenter: DirectComposition + DXGI (GPU interop, full click-through)");
        return result;
    } catch (const std::bad_alloc &) {
        return SDL_SetError("Cannot allocate DirectComposition frame");
    }
}

extern "C" bool bongo_cat_windows_composition_opacity(void *state, float opacity) {
    auto *p = static_cast<Presenter *>(state);
    if (!p || !p->effect || !p->composition) return false;
    HRESULT hr = p->effect->SetOpacity(opacity);
    if (SUCCEEDED(hr)) hr = p->composition->Commit();
    if (FAILED(hr)) return SDL_SetError("Cannot update DirectComposition opacity (0x%08lx)",
        static_cast<unsigned long>(hr));
    p->opacity = opacity;
    return true;
}

extern "C" void bongo_cat_windows_composition_destroy(void *state) {
    auto *p = static_cast<Presenter *>(state);
    if (!p) return;
    if (p->target) p->target->SetRoot(nullptr);
    if (p->composition) p->composition->Commit();
    SDL_Window *previous_window = SDL_GL_GetCurrentWindow();
    SDL_GLContext previous_context = SDL_GL_GetCurrentContext();
    if (p->gl_context && SDL_GL_MakeCurrent(p->gl_window, p->gl_context)) {
        if (!release_texture(*p)) SDL_LogWarn(BONGO_CAT_LOG_LIFECYCLE, "%s", SDL_GetError());
        if (p->resolve_color) p->delete_renderbuffers(1, &p->resolve_color);
        if (p->resolve_framebuffer) p->delete_framebuffers(1, &p->resolve_framebuffer);
        if (p->interop && !p->close_device(p->interop))
            SDL_LogWarn(BONGO_CAT_LOG_LIFECYCLE, "Cannot close DirectComposition GPU interop device");
        if (previous_context != p->gl_context &&
            !SDL_GL_MakeCurrent(previous_window, previous_context))
            SDL_LogWarn(BONGO_CAT_LOG_LIFECYCLE, "Cannot restore OpenGL context after DirectComposition cleanup");
    }
    delete p;
}
