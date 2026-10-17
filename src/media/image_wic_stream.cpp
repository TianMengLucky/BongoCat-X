#ifdef _WIN32
#include "image_wic_internal.hpp"
#include <SDL3/SDL.h>
#include <limits.h>
using bongo_cat::ComPtr;
#include <wincodec.h>
#include <windows.h>

typedef struct WicRowJob {
    const char *path;
    BongoCatImageRows consume;
    void *consumer;
    BongoCatImageProgress progress;
    void *userdata;
    SDL_Semaphore *ready, *consumed;
    BongoCatImage rows;
    int height, y;
    bool keep_decoding, finished, result;
} WicRowJob;

static int SDLCALL decode_rows(void *userdata) {
    WicRowJob *job = static_cast<WicRowJob *>(userdata);
    bongo_cat::ComApartment apartment(COINIT_MULTITHREADED);
    HRESULT initialized = apartment.status();
    HRESULT status = initialized == RPC_E_CHANGED_MODE ? S_OK : initialized;
    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> converter;
    auto wide = bongo_cat::wic_path(job->path);
    if (!wide) status = E_FAIL;
    if (SUCCEEDED(status))
        status = CoCreateInstance(CLSID_WICImagingFactory, NULL,
            CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.GetAddressOf()));
    if (SUCCEEDED(status))
        status = factory->CreateDecoderFromFilename(wide.get(),
            NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, decoder.GetAddressOf());
    if (SUCCEEDED(status)) status = decoder->GetFrame(0, frame.GetAddressOf());
    UINT width = 0, height = 0;
    if (SUCCEEDED(status)) status = frame->GetSize(&width, &height);
    if (!width || !height || width > INT_MAX / 4 || height > INT_MAX) status = E_FAIL;
    if (SUCCEEDED(status))
        status = factory->CreateFormatConverter(converter.GetAddressOf());
    if (SUCCEEDED(status))
        status = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone, NULL, 0, WICBitmapPaletteTypeCustom);

    /* One strip crosses the worker/caller boundary. The decoder cannot reuse
       it until the GL upload has consumed it. At 8192 pixels this is 2 MiB,
       instead of another complete 256 MiB atlas. */
    UINT stride = width * 4;
    UINT batch = stride ? SDL_min(height,
        SDL_max(1u, SDL_min(64u, 4u * 1024u * 1024u / stride))) : 0;
    bongo_cat::ImageMemory<unsigned char> pixels(SUCCEEDED(status) ?
        static_cast<unsigned char *>(std::malloc((size_t)stride * batch)) : nullptr);
    if (!pixels) status = E_OUTOFMEMORY;
    for (UINT y = 0; SUCCEEDED(status) && y < height; y += batch) {
        UINT rows = SDL_min(batch, height - y);
        WICRect rect = {0, (INT)y, (INT)width, (INT)rows};
        status = converter->CopyPixels(&rect,
            stride, stride * rows, pixels.get());
        if (FAILED(status)) break;
        job->rows = {};
        job->rows.pixels = pixels.get(); job->rows.width = (int)width; job->rows.height = (int)rows;
        job->height = (int)height;
        job->y = (int)y;
        if (job->ready) {
            SDL_SignalSemaphore(job->ready);
            SDL_WaitSemaphore(job->consumed);
        } else {
            job->keep_decoding = job->consume(job->consumer,
                &job->rows, job->height, job->y);
            if (job->progress)
                job->progress(job->userdata, (float)(y + rows) / height);
        }
        if (!job->keep_decoding) { status = E_ABORT; break; }
    }
    job->result = SUCCEEDED(status);
    job->finished = true;
    if (job->ready) SDL_SignalSemaphore(job->ready);
    return job->result ? 0 : -1;
}

struct WicThreadJoin {
    void operator()(SDL_Thread *thread) const noexcept { SDL_WaitThread(thread, nullptr); }
};

bool bongo_cat_image_decode_wic_rows(const char *path,
    BongoCatImageRows consume, void *consumer,
    BongoCatImageProgress progress, void *userdata) {
    if (!path || !consume) return false;
    WicRowJob job{};
    job.path = path; job.consume = consume; job.consumer = consumer;
    job.progress = progress; job.userdata = userdata; job.keep_decoding = true;
    using Semaphore = std::unique_ptr<SDL_Semaphore, decltype(&SDL_DestroySemaphore)>;
    Semaphore ready(SDL_CreateSemaphore(0), SDL_DestroySemaphore);
    Semaphore consumed(SDL_CreateSemaphore(0), SDL_DestroySemaphore);
    job.ready = ready.get(); job.consumed = consumed.get();
    // Join before semaphore and stack-job destruction, including failure exits.
    std::unique_ptr<SDL_Thread, WicThreadJoin> worker(job.ready && job.consumed ?
        SDL_CreateThread(decode_rows, BONGO_CAT_SLUG "-wic-rows", &job) : nullptr);
    if (!worker) {
        job.ready = job.consumed = NULL;
        decode_rows(&job);
        return job.result;
    }
    float fraction = 0.0f;
    for (;;) {
        if (SDL_WaitSemaphoreTimeout(job.ready, 2)) {
            if (job.finished) break;
            job.keep_decoding = consume(consumer, &job.rows, job.height, job.y);
            fraction = (float)(job.y + job.rows.height) / job.height;
            SDL_SignalSemaphore(job.consumed);
        }
        /* UI work and all GL calls remain on the caller's context. */
        if (progress) progress(userdata, fraction);
    }
    worker.reset();
    return job.result;
}
#endif
