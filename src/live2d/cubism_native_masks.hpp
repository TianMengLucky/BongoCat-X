#pragma once
#include "cubism_mask_policy.hpp"
namespace bongo_cat {
struct NativeMaskSizes { MaskSize drawable, offscreen; };
inline thread_local NativeMaskSizes native_mask_sizes;
struct NativeMaskCounts { int drawable = 0, offscreen = 0; };
inline thread_local NativeMaskCounts native_mask_counts;
class NativeMaskCountScope {
public:
    NativeMaskCountScope(int drawable, int offscreen) : previous_(native_mask_counts) {
        native_mask_counts = {drawable, offscreen};
    }
    ~NativeMaskCountScope() { native_mask_counts = previous_; }
private:
    NativeMaskCounts previous_;
};
class NativeMaskScope {
public:
    NativeMaskScope(MaskSize drawable, MaskSize offscreen) : previous_(native_mask_sizes) {
        native_mask_sizes = {drawable, offscreen};
    }
    ~NativeMaskScope() { native_mask_sizes = previous_; }
private:
    NativeMaskSizes previous_;
};
}
