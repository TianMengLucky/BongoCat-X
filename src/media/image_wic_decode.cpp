#ifdef _WIN32
#include "image_wic_internal.hpp"
#include <SDL3/SDL.h>
#include <limits.h>
#include <cstring>
using bongo_cat::ComPtr;

static bool decode_wic(const char *path, BongoCatImage *image,
    UINT max_width, UINT max_height, BongoCatImageProgress progress,
    void *userdata, BongoCatImageCancelled cancelled, void *cancel_data) {
    if (!path || !image) return false;
    *image = {};
    if (cancelled && cancelled(cancel_data)) return false;
    bongo_cat::ComApartment apartment(COINIT_MULTITHREADED);
    HRESULT initialized = apartment.status();
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) return false;
    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICBitmapScaler> scaler;
    ComPtr<IWICFormatConverter> premultiplied;
    ComPtr<IWICFormatConverter> converter;
    auto wide = bongo_cat::wic_path(path);
    HRESULT result = wide ? CoCreateInstance(CLSID_WICImagingFactory, NULL,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.GetAddressOf())) : E_FAIL;
    if (SUCCEEDED(result))
        result = factory->CreateDecoderFromFilename(wide.get(),
            NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, decoder.GetAddressOf());
    if (SUCCEEDED(result)) result = decoder->GetFrame(0, frame.GetAddressOf());
    UINT source_width = 0, source_height = 0;
    if (SUCCEEDED(result))
        result = frame->GetSize(&source_width, &source_height);
    if (!source_width || !source_height || source_width > INT_MAX / 4 ||
        source_height > INT_MAX) result = E_FAIL;
    if (cancelled && cancelled(cancel_data)) result = E_ABORT;
    UINT target_width = source_width, target_height = source_height;
    if (max_width && max_height &&
        (source_width > max_width || source_height > max_height)) {
        if ((uint64_t)max_width * source_height <=
            (uint64_t)max_height * source_width) {
            target_width = max_width;
            target_height = (UINT)((uint64_t)source_height * max_width /
                source_width);
        } else {
            target_height = max_height;
            target_width = (UINT)((uint64_t)source_width * max_height /
                source_height);
        }
        if (!target_width) target_width = 1;
        if (!target_height) target_height = 1;
        /* Keep transparent RGB out of the resize filter. Only the output is
           explicitly bounded here; the codec may retain source storage. */
        if (SUCCEEDED(result))
            result = factory->CreateFormatConverter(premultiplied.GetAddressOf());
        if (SUCCEEDED(result))
            result = premultiplied->Initialize(frame.Get(), GUID_WICPixelFormat32bppPRGBA,
                WICBitmapDitherTypeNone, NULL, 0, WICBitmapPaletteTypeCustom);
        if (SUCCEEDED(result))
            result = factory->CreateBitmapScaler(scaler.GetAddressOf());
        if (SUCCEEDED(result))
            result = scaler->Initialize(premultiplied.Get(), target_width, target_height,
                WICBitmapInterpolationModeFant);
    }
    IWICBitmapSource *source = scaler ? static_cast<IWICBitmapSource *>(scaler.Get()) :
        static_cast<IWICBitmapSource *>(frame.Get());
    if (SUCCEEDED(result))
        result = factory->CreateFormatConverter(converter.GetAddressOf());
    if (SUCCEEDED(result))
        result = converter->Initialize(source,
            GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, NULL, 0,
            WICBitmapPaletteTypeCustom);
    size_t stride = target_width <= SIZE_MAX / 4
        ? (size_t)target_width * 4 : 0;
    size_t bytes = stride && target_height <= SIZE_MAX / stride
        ? stride * target_height : 0;
    if (cancelled && cancelled(cancel_data)) result = E_ABORT;
    bongo_cat::ImageMemory<unsigned char> pixels(SUCCEEDED(result) && bytes && bytes <= UINT_MAX
        ? static_cast<unsigned char *>(std::malloc(bytes)) : nullptr);
    if (SUCCEEDED(result) && pixels) {
        for (UINT y = 0; y < target_height && SUCCEEDED(result); y += 256) {
            if (cancelled && cancelled(cancel_data)) { result = E_ABORT; break; }
            UINT rows = SDL_min(256u, target_height - y);
            WICRect rect = {0, (INT)y, (INT)target_width, (INT)rows};
            result = converter->CopyPixels(&rect,
                (UINT)stride, (UINT)(stride * rows),
                pixels.get() + (size_t)y * stride);
            if (progress)
                progress(userdata,
                    (float)(y + rows) / (float)target_height);
        }
    }
    bool ok = SUCCEEDED(result) && pixels && !(cancelled && cancelled(cancel_data));
    if (ok) {
        image->pixels = pixels.release();
        image->width = (int)target_width;
        image->height = (int)target_height;
    }
    return ok;
}

typedef struct WicDecodeJob {
    const char *path = nullptr;
    BongoCatImage *image = nullptr;
    UINT max_width = 0;
    UINT max_height = 0;
    bool result = false;
} WicDecodeJob;

static int SDLCALL decode_wic_worker(void *userdata) {
    WicDecodeJob *job = static_cast<WicDecodeJob *>(userdata);
    job->result = decode_wic(job->path, job->image,
        job->max_width, job->max_height, NULL, NULL, NULL, NULL);
    return job->result ? 0 : -1;
}

bool bongo_cat_image_decode_wic_responsive(const char *path,
    BongoCatImage *image, int max_width, int max_height,
    BongoCatImageProgress progress, void *userdata) {
    if (!progress)
        return decode_wic(path, image, (UINT)max_width,
            (UINT)max_height, NULL, NULL, NULL, NULL);
    WicDecodeJob job = {path, image, (UINT)max_width, (UINT)max_height, false};
    SDL_Thread *worker = SDL_CreateThread(decode_wic_worker,
        BONGO_CAT_SLUG "-wic-decode", &job);
    if (!worker)
        return decode_wic(path, image, (UINT)max_width,
            (UINT)max_height, progress, userdata, NULL, NULL);
    bongo_cat_image_wait_for_decode(worker, progress, userdata);
    return job.result;
}

bool bongo_cat_image_decode_wic_cancellable(const char *path,
    BongoCatImage *image, int max_width, int max_height,
    BongoCatImageCancelled cancelled, void *cancel_data) {
    return decode_wic(path, image, (UINT)max_width, (UINT)max_height,
        NULL, NULL, cancelled, cancel_data);
}
#endif
