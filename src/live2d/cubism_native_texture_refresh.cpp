#include "cubism_plugin_services.hpp"
#include "cubism_model.hpp"
#include "cubism_texture_resolution.hpp"
#include "bongo_cat/safe_ffi.h"
#include "bongo_cat/log.h"
#include <SDL3/SDL_thread.h>
#include <SDL3/SDL_timer.h>
#include <atomic>
#include <exception>
#include <new>

namespace bongo_cat {
struct NativeTextureRefresh {
    NativeModel *owner = nullptr;
    SDL_Thread *thread = nullptr;
    std::string file;
    TextureResolution bound{};
    BongoCatImage image{};
    BongoCatImageAlphaMask alpha{};
    BongoCatError error{};
    size_t index = 0;
    int source_width = 0, source_height = 0;
    uint64_t paused_ns = 0;
    std::atomic<bool> cancelled{false}, done{false};
    bool success = false;
    ~NativeTextureRefresh() {
        cancelled.store(true);
        if (thread) SDL_WaitThread(thread, nullptr);
        bongo_cat_image_free(&image);
    }
};

void NativeModel::cancel_native_texture_refresh(bool wait) {
    if (!native_texture_refresh_) return;
    native_texture_refresh_->cancelled.store(true);
    if (!wait) return;
    delete native_texture_refresh_;
    native_texture_refresh_ = nullptr;
}

bool NativeModel::native_texture_refresh_busy() const {
    return native_texture_refresh_ && !native_texture_refresh_->paused_ns;
}

bool NativeModel::native_texture_refresh_due(bool active, bool allow_start) const {
    const auto now = SDL_GetTicksNS();
    if (native_texture_refresh_) {
        const auto &job = *native_texture_refresh_;
        if (job.cancelled.load()) return job.done.load();
        if (!active) return !job.paused_ns || now - job.paused_ns >= 5000000000ull;
        return job.paused_ns || (job.done.load() && now - texture_resize_ns_ >= 200000000ull);
    }
    return active && allow_start && dynamic_texture_resolution_ &&
        texture_refresh_pending_ && now - texture_resize_ns_ >= 200000000ull;
}

bool NativeModel::refresh_native_texture_resolution(bool active, bool allow_start) {
    if (!native_texture_refresh_due(active, allow_start)) return false;
    const auto now = SDL_GetTicksNS();
    if (native_texture_refresh_) {
        auto &job = *native_texture_refresh_;
        if (!active && !job.cancelled.load()) {
            if (!job.paused_ns) job.paused_ns = now;
            if (now - job.paused_ns < 5000000000ull) return false;
            job.cancelled.store(true);
        }
        if (active && job.paused_ns && !job.cancelled.load()) {
            job.paused_ns = 0;
            texture_resize_ns_ = now;
            return false;
        }
        if (!job.done.load()) return false;
        bool changed = false;
        const int width = job.image.width, height = job.image.height;
        if (!job.cancelled.load() && job.success && active) {
            try {
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
                if (rhi_info_.backend == BONGO_CAT_RHI_VULKAN)
                    changed = upload_texture_vulkan((int)job.index, job.image.pixels,
                        job.image.width, job.image.height, &job.error);
#endif
#ifdef BONGO_CAT_HAS_CUBISM_METAL
                if (rhi_info_.backend == BONGO_CAT_RHI_METAL) {
                    changed = upload_texture_metal((int)job.index, job.image.pixels,
                        width, height, &job.error);
                }
#endif
            } catch (const std::exception &exception) {
                bongo_cat_error_set(&job.error, BONGO_CAT_ERROR_MEMORY, "%s", exception.what());
            }
            if (changed) {
                auto &metadata = native_texture_info_[job.index];
                native_texture_bytes_ -= (size_t)metadata.width * (size_t)metadata.height * 4;
                metadata.width = width; metadata.height = height;
                metadata.source_width = job.source_width;
                metadata.source_height = job.source_height;
                native_texture_bytes_ += (size_t)metadata.width * (size_t)metadata.height * 4;
                native_alpha_[job.index] = job.alpha;
                triangle_alpha_.clear();
                try { prepare_frame_bounds(); }
                catch (const std::bad_alloc &) { frame_drawables_.clear(); }
                visual_state_cached_ = false;
                SDL_LogInfo(BONGO_CAT_LOG_LIFECYCLE,
                    "[texture-refresh] native backend=%u texture=%u size=%dx%d",
                    (unsigned)rhi_info_.backend, (unsigned)job.index, metadata.width, metadata.height);
            }
        }
        if (!job.cancelled.load()) {
            texture_refresh_index_ = job.index + 1;
            if (!changed) SDL_LogWarn(SDL_LOG_CATEGORY_RENDER,
                "[texture-refresh] Keeping current native texture: %s", job.error.message);
        }
        delete native_texture_refresh_;
        native_texture_refresh_ = nullptr;
        return changed;
    }
    if (!active || !allow_start || !dynamic_texture_resolution_ || !setting_) return false;
    for (; texture_refresh_index_ < native_texture_info_.size(); ++texture_refresh_index_) {
        const auto &metadata = native_texture_info_[texture_refresh_index_];
        const auto bound = texture_refresh_bound(metadata, texture_limit_, render_quality_percent_);
        const auto fitted = texture_fitted_size(metadata.source_width, metadata.source_height, bound);
        if (fitted == std::make_pair(metadata.width, metadata.height)) continue;
        if (fitted.first <= metadata.width && fitted.second <= metadata.height &&
            now - texture_resize_ns_ < 300000000ull) return false;
        std::unique_ptr<NativeTextureRefresh> job;
        try {
            job = std::make_unique<NativeTextureRefresh>();
            job->owner = this; job->index = texture_refresh_index_; job->bound = bound;
            job->file = path(setting_->GetTextureFileName((int)job->index));
        } catch (const std::bad_alloc &) {
            texture_refresh_pending_ = false;
            return false;
        }
        job->thread = SDL_CreateThread([](void *userdata) -> int {
            auto &work = *static_cast<NativeTextureRefresh *>(userdata);
            BongoCatImage source{};
            try {
                {
                    auto bytes = work.owner->read(work.file);
                    if (!work.cancelled.load()) source.pixels = bongo_safe_image_decode(
                        bytes.data(), bytes.size(), &source.width, &source.height);
                }
                source.pixels_ffi = true;
                if (source.pixels && !work.cancelled.load()) {
                    work.source_width = source.width; work.source_height = source.height;
                    bongo_cat_image_make_alpha_mask(&source, &work.alpha);
                    work.success = bongo_cat_image_resize_rgba_take(&source,
                        work.bound.max_width, work.bound.max_height, &work.image, &work.error);
                    if (work.success && !work.cancelled.load()) {
                        const size_t count = (size_t)work.image.width * (size_t)work.image.height * 4;
                        for (size_t p = 0; p < count; p += 4) {
                            if ((p & 65535u) == 0 && work.cancelled.load()) break;
                            const unsigned alpha = work.image.pixels[p + 3];
                            for (size_t c = 0; c < 3; ++c)
                                work.image.pixels[p + c] = (unsigned char)(work.image.pixels[p + c] * alpha / 255);
                        }
                    }
                } else if (!work.cancelled.load())
                    bongo_cat_error_set(&work.error, BONGO_CAT_ERROR_IO, "Cannot decode native texture");
            } catch (const std::exception &exception) {
                bongo_cat_error_set(&work.error, BONGO_CAT_ERROR_MEMORY, "%s", exception.what());
            }
            bongo_cat_image_free(&source);
            work.done.store(true);
            return 0;
        }, BONGO_CAT_SLUG "-native-texture", job.get());
        if (!job->thread) {
            texture_refresh_pending_ = false;
            return false;
        }
        native_texture_refresh_ = job.release();
        return false;
    }
    texture_refresh_pending_ = false;
    return false;
}
} // namespace bongo_cat
