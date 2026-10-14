// Windows-only C++ bridge; the host and public presenter interface remain C11.
#include "windows_composition.h"
#include "bongo_cat/log.h"
#include <SDL3/SDL.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <wrl/client.h>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>

using Microsoft::WRL::ComPtr;
namespace {
struct Presenter {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain1> swap;
    ComPtr<IDCompositionDevice> composition;
    ComPtr<IDCompositionTarget> target;
    ComPtr<IDCompositionVisual> visual;
    std::vector<unsigned char> upload;
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
        FAILED(hr = p.visual->SetContent(p.swap.Get())) ||
        FAILED(hr = p.target->SetRoot(p.visual.Get()))) return hr;
    p.width = width;
    p.height = height;
    LONG_PTR style = GetWindowLongPtrW(window, GWL_EXSTYLE);
    SetLastError(ERROR_SUCCESS);
    if (!SetWindowLongPtrW(window, GWL_EXSTYLE,
            (style & ~WS_EX_LAYERED) | WS_EX_NOREDIRECTIONBITMAP) && GetLastError())
        return HRESULT_FROM_WIN32(GetLastError());
    if (!SetWindowPos(window, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE |
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED))
        return HRESULT_FROM_WIN32(GetLastError());
    return S_OK;
}

// User32 cannot alpha-hit-test composition visuals. A native window region
// keeps blank pixels clickable across processes, unlike HTTRANSPARENT alone.
bool update_region(HWND window, const Presenter &p) {
    std::vector<RECT> runs;
    for (int y = 0; y < p.height; ++y) {
        const unsigned char *row = p.upload.data() + static_cast<size_t>(y) * p.width * 4;
        int x = 0;
        while (x < p.width) {
            while (x < p.width && row[static_cast<size_t>(x) * 4 + 3] <= 8) ++x;
            int first = x;
            while (x < p.width && row[static_cast<size_t>(x) * 4 + 3] > 8) ++x;
            if (first < x) runs.push_back(RECT{first, y, x, y + 1});
        }
    }
    HRGN region = nullptr;
    if (runs.empty()) region = CreateRectRgn(0, 0, 0, 0);
    else {
        size_t bytes = sizeof(RGNDATAHEADER) + runs.size() * sizeof(RECT);
        if (bytes > (std::numeric_limits<DWORD>::max)())
            return SDL_SetError("DirectComposition input region is too large");
        // DWORD storage keeps RGNDATA naturally aligned.
        std::vector<DWORD> storage((bytes + sizeof(DWORD) - 1) / sizeof(DWORD));
        auto *data = reinterpret_cast<RGNDATA *>(storage.data());
        data->rdh.dwSize = sizeof(RGNDATAHEADER);
        data->rdh.iType = RDH_RECTANGLES;
        data->rdh.nCount = static_cast<DWORD>(runs.size());
        data->rdh.nRgnSize = static_cast<DWORD>(runs.size() * sizeof(RECT));
        data->rdh.rcBound = RECT{0, 0, p.width, p.height};
        std::memcpy(data->Buffer, runs.data(), runs.size() * sizeof(RECT));
        region = ExtCreateRegion(nullptr, static_cast<DWORD>(bytes), data);
    }
    if (!region) return SDL_SetError("Cannot create DirectComposition input region");
    HRGN previous = CreateRectRgn(0, 0, 0, 0);
    bool same = previous && GetWindowRgn(window, previous) != ERROR && EqualRgn(previous, region);
    if (previous) DeleteObject(previous);
    if (same) { DeleteObject(region); return true; }
    if (!SetWindowRgn(window, region, FALSE)) {
        DeleteObject(region);
        return SDL_SetError("Cannot apply DirectComposition input region");
    }
    return true; // User32 owns region after SetWindowRgn succeeds.
}

bool present(Presenter &p, HWND window, const RECT &bounds,
    const unsigned char *pixels, int width, int height, float opacity) {
    HRESULT hr;
    if (!p.device) {
        hr = initialize(p, window, width, height);
        if (FAILED(hr)) return SDL_SetError("DirectComposition initialization failed (0x%08lx)",
            static_cast<unsigned long>(hr));
    }
    if (p.width != width || p.height != height) {
        hr = p.swap->ResizeBuffers(0, static_cast<UINT>(width), static_cast<UINT>(height),
            DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(hr)) return SDL_SetError("DXGI resize failed (0x%08lx)", static_cast<unsigned long>(hr));
        p.width = width; p.height = height;
    }
    size_t stride = static_cast<size_t>(width) * 4;
    p.upload.resize(stride * static_cast<size_t>(height));
    // Existing DIB/readback frames are bottom-up premultiplied BGRA.
    for (int y = 0; y < height; ++y)
        std::memcpy(p.upload.data() + static_cast<size_t>(y) * stride,
            pixels + static_cast<size_t>(height - 1 - y) * stride, stride);
    if (!update_region(window, p)) return false;
    RECT current;
    if (!GetWindowRect(window, &current)) return SDL_SetError("Cannot locate DXGI window");
    if (current.left != bounds.left || current.top != bounds.top ||
        current.right - current.left != width || current.bottom - current.top != height) {
        if (!SetWindowPos(window, nullptr, bounds.left, bounds.top, width, height,
                SWP_NOZORDER | SWP_NOACTIVATE)) return SDL_SetError("Cannot position DXGI window");
    }
    ComPtr<ID3D11Texture2D> buffer;
    hr = p.swap->GetBuffer(0, IID_PPV_ARGS(buffer.GetAddressOf()));
    if (FAILED(hr)) return SDL_SetError("DXGI back buffer failed (0x%08lx)", static_cast<unsigned long>(hr));
    p.context->UpdateSubresource(buffer.Get(), 0, nullptr, p.upload.data(), static_cast<UINT>(stride), 0);
    if (!p.committed || p.opacity != opacity) {
        hr = p.visual->SetOpacity(opacity);
        if (SUCCEEDED(hr)) hr = p.composition->Commit();
        if (FAILED(hr)) return SDL_SetError("DirectComposition commit failed (0x%08lx)", static_cast<unsigned long>(hr));
        p.opacity = opacity;
        p.committed = true;
    }
    hr = p.swap->Present(1, 0);
    if (FAILED(hr)) return SDL_SetError("DXGI present failed (0x%08lx)", static_cast<unsigned long>(hr));
    return true;
}
} // namespace

extern "C" bool bongo_cat_windows_composition_present(void **state, HWND window,
    const RECT *bounds, const unsigned char *pixels, int width, int height, float opacity) {
    if (!state || !window || !bounds || !pixels || width <= 0 || height <= 0 ||
        width > (std::numeric_limits<int>::max)() / 4 ||
        static_cast<size_t>(height) > (std::numeric_limits<size_t>::max)() /
            (static_cast<size_t>(width) * 4)) return SDL_SetError("Invalid DXGI frame");
    if (SDL_GetHintBoolean("BONGO_CAT_TEST_COMPOSITION_FAILURE", false))
        return SDL_SetError("Injected DirectComposition failure");
    try {
        bool created = !*state;
        if (created) *state = new Presenter;
        bool result = present(*static_cast<Presenter *>(*state), window, *bounds,
            pixels, width, height, opacity);
        if (result && created) SDL_LogInfo(BONGO_CAT_LOG_LIFECYCLE,
            "Windows presenter: DirectComposition + DXGI (pixel readback)");
        return result;
    } catch (const std::bad_alloc &) {
        return SDL_SetError("Cannot allocate DirectComposition frame");
    } catch (const std::length_error &) {
        return SDL_SetError("DirectComposition frame exceeds container capacity");
    }
}

extern "C" void bongo_cat_windows_composition_destroy(void *state) {
    auto *p = static_cast<Presenter *>(state);
    if (!p) return;
    if (p->target) p->target->SetRoot(nullptr);
    if (p->composition) p->composition->Commit();
    delete p;
}
