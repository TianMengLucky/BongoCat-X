#ifndef BONGO_CAT_IMAGE_BC7_H
#define BONGO_CAT_IMAGE_BC7_H
#include "bongo_cat/image.h"
#include "bongo_cat/safe_ffi.h"
bool bongo_cat_image_bc7_supported(void);
/* Calling GL thread, full mip chain required. Failure destroys partial storage. */
unsigned int bongo_cat_image_upload_bc7(const BongoSafeNativeTexture *image,
    BongoCatError *error);
#endif
