#pragma once
#include "cubism_plugin_services.hpp"
#include "bongo_cat/safe_ffi.h"
#include "bongo_cat/image.h"
#include <algorithm>
#include <cstring>
namespace bongo_cat {
struct NativeTexturePixels {
    BongoSafeNativeTexture data{};
    ~NativeTexturePixels() { clear(); }
    NativeTexturePixels() = default;
    NativeTexturePixels(const NativeTexturePixels &) = delete;
    NativeTexturePixels &operator=(const NativeTexturePixels &) = delete;
    void clear() {
        if (data.pixels) bongo_safe_free_pixels(data.pixels, data.size);
        data = {};
    }
    bool load(const char *path, int width, int height, bool bc7, BongoCatError *error,
        bongo_safe_cancelled cancelled = nullptr, void *userdata = nullptr) {
        clear();
        if (bongo_safe_native_texture_load(path, (uint32_t)width, (uint32_t)height, bc7,
                &data, cancelled, userdata))
            return true;
        if (bc7 && (!cancelled || !cancelled(userdata))) {
            clear();
            if (bongo_safe_native_texture_load(path, (uint32_t)width, (uint32_t)height, false,
                    &data, cancelled, userdata)) return true;
        }
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Cannot decode native texture: %s", path);
        return false;
    }
    void alpha(BongoCatImageAlphaMask &mask) const {
        std::memcpy(mask.pixels, data.alpha, sizeof(mask.pixels));
        mask.width = (int)std::min(data.source_width, 128u);
        mask.height = (int)std::min(data.source_height, 128u);
    }
};
}
