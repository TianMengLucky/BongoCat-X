#ifndef BONGO_CAT_CUBISM_RUNTIME_HPP
#define BONGO_CAT_CUBISM_RUNTIME_HPP

#include "cubism_model.hpp"

#include "bongo_cat/rhi.h"

struct BongoCatLive2D {
    bongo_cat::NativeModel *model;
    int width = 612;
    int height = 354;
    /* Device handles of the active frame backend, attached by the runtime
       after create and on hot switches (see docs/live2d-vulkan-metal.md). */
    BongoCatRhiDeviceInfo rhi_info = {};
};

#endif
