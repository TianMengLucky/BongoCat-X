#include "cubism_plugin_services.hpp"
#include "cubism_model.hpp"
#include "cubism_native_texture.hpp"
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
    SDL_Thread *thread = nullptr;
    std::string file;
    TextureResolution bound{};
    NativeTexturePixels atlas;
    bool bc7 = false;
    BongoCatError error{};
    size_t index = 0;
    uint64_t paused_ns = 0;
    std::atomic<bool> cancelled{false}, done{false};
    bool success = false;
    ~NativeTextureRefresh() {
        cancelled.store(true);
        if (thread) SDL_WaitThread(thread, nullptr);
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
        if (!job.cancelled.load() && job.success && active) {
            try {
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
                if (rhi_info_.backend == BONGO_CAT_RHI_VULKAN)
                    changed = upload_texture_vulkan((int)job.index, job.atlas.data.pixels,
                        (int)job.atlas.data.width, (int)job.atlas.data.height, &job.error,
                        job.atlas.data.format, job.atlas.data.size);
                if (!changed && rhi_info_.backend == BONGO_CAT_RHI_VULKAN && job.atlas.data.format == 1 &&
                    job.atlas.load(job.file.c_str(), job.bound.max_width, job.bound.max_height, false, &job.error))
                    changed = upload_texture_vulkan((int)job.index, job.atlas.data.pixels,
                        (int)job.atlas.data.width, (int)job.atlas.data.height, &job.error,
                        job.atlas.data.format, job.atlas.data.size);
#endif
#ifdef BONGO_CAT_HAS_CUBISM_METAL
                if (rhi_info_.backend == BONGO_CAT_RHI_METAL) {
                    changed = upload_texture_metal((int)job.index, job.atlas.data.pixels,
                        (int)job.atlas.data.width, (int)job.atlas.data.height, &job.error);
                }
#endif
            } catch (const std::exception &exception) {
                bongo_cat_error_set(&job.error, BONGO_CAT_ERROR_MEMORY, "%s", exception.what());
            }
            if (changed) {
                auto &metadata = native_texture_info_[job.index];
                native_texture_bytes_ -= metadata.bytes;
                metadata.width = (int)job.atlas.data.width; metadata.height = (int)job.atlas.data.height;
                metadata.source_width = (int)job.atlas.data.source_width;
                metadata.source_height = (int)job.atlas.data.source_height;
                metadata.bytes = job.atlas.data.size;
                native_texture_bytes_ += metadata.bytes;
                job.atlas.alpha(native_alpha_[job.index]);
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
            job->index = texture_refresh_index_; job->bound = bound;
            job->bc7 = rhi_info_.backend == BONGO_CAT_RHI_VULKAN &&
                (native_vulkan_features_ & BONGO_CAT_VULKAN_TEXTURE_COMPRESSION_BC) != 0;
            job->file = path(setting_->GetTextureFileName((int)job->index));
        } catch (const std::bad_alloc &) {
            texture_refresh_pending_ = false;
            return false;
        }
        job->thread = SDL_CreateThread([](void *userdata) -> int {
            auto &work = *static_cast<NativeTextureRefresh *>(userdata);
            try {
                if (!work.cancelled.load())
                    work.success = work.atlas.load(work.file.c_str(), work.bound.max_width,
                        work.bound.max_height, work.bc7, &work.error,
                        [](void *data) -> bool {
                            return static_cast<NativeTextureRefresh *>(data)->cancelled.load();
                        }, &work);
            } catch (const std::exception &exception) {
                bongo_cat_error_set(&work.error, BONGO_CAT_ERROR_MEMORY, "%s", exception.what());
            }
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
