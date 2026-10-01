#ifndef BONGO_CAT_PREFERENCES_ABOUT_SVG_H
#define BONGO_CAT_PREFERENCES_ABOUT_SVG_H

/* SVG input is modified in place. Pixel buffers are released with free(). */

/* Bounded feed parsing uses the same XML tokenizer as SVG rasterization. */
int bongo_cat_about_svg_parse_xml(char *input,
    void (*start)(void *, const char *, const char **),
    void (*end)(void *, const char *), void (*content)(void *, const char *), void *user);

#endif
