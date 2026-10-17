#ifndef BONGO_CAT_IMAGE_WIC_INTERNAL_HPP
#define BONGO_CAT_IMAGE_WIC_INTERNAL_HPP

#include "image_internal.h"
#include "../platform/windows/windows_com.hpp"
#include <cstdlib>
#include <wincodec.h>

namespace bongo_cat {
struct ImageHeapFree {
    void operator()(void *p) const noexcept { std::free(p); }
};
template<class T> using ImageMemory = std::unique_ptr<T, ImageHeapFree>;

inline ImageMemory<wchar_t> wic_path(const char *path) {
    int count = path ? MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        path, -1, nullptr, 0) : 0;
    ImageMemory<wchar_t> wide(count > 0 ?
        static_cast<wchar_t *>(std::malloc((size_t)count * sizeof(wchar_t))) : nullptr);
    if (wide && !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        path, -1, wide.get(), count)) wide.reset();
    return wide;
}
}
#endif
