#pragma once
#include "cubism_mask_policy.hpp"
namespace bongo_cat {
struct NativeMaskSizes { MaskSize drawable, offscreen; };
inline thread_local NativeMaskSizes native_mask_sizes;
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
