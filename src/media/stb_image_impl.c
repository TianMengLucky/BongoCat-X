/* Image decoding lives in bongo-safe. Reuse miniz for PNG compression
   instead of embedding stb's second zlib implementation. */
#include <miniz.h>
#include <limits.h>
#include <stdlib.h>

static unsigned char *compress_png(unsigned char *data, int size,
    int *output_size, int quality) {
    if (!data || size < 0 || !output_size) return NULL;
    *output_size = 0;
    mz_ulong capacity = mz_compressBound((mz_ulong)size);
    if (!capacity || capacity > INT_MAX) return NULL;
    unsigned char *compressed = malloc((size_t)capacity);
    if (!compressed) return NULL;
    int level = quality < 0 ? MZ_DEFAULT_COMPRESSION :
        (quality > 9 ? 9 : quality);
    if (mz_compress2(compressed, &capacity, data, (mz_ulong)size, level) != MZ_OK ||
        capacity > INT_MAX) {
        free(compressed);
        return NULL;
    }
    *output_size = (int)capacity;
    return compressed;
}

/* All callers write through UTF-8-aware file callbacks. */
#define STBI_WRITE_NO_STDIO
#define STBIW_ZLIB_COMPRESS compress_png
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
