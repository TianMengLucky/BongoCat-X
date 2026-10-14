#ifndef BONGO_CAT_CUBISM_TEXTURE_DIMENSIONS_HPP
#define BONGO_CAT_CUBISM_TEXTURE_DIMENSIONS_HPP

namespace bongo_cat {
/* Native backends retain dimensions separately from their alpha masks. */
struct TextureDimensions {
    int width = 0, height = 0;
    int source_width = 0, source_height = 0;
};
}

#endif
