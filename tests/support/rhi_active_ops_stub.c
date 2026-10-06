/* Unit-test stub: the readback tests exercise the legacy OpenGL path and
   never attach a frame backend. */
#include "bongo_cat/rhi.h"

const BongoCatRhiPresentOps *bongo_cat_rhi_active_present_ops(void) {
    return NULL;
}

bool bongo_cat_rhi_active_is_gl(void) {
    return true;
}
