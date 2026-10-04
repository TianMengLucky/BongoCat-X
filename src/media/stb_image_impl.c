/* Image decoding moved to the bongo-safe Rust crate; stb remains only for
   encoding (PNG covers/screenshots) and resizing. */
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
