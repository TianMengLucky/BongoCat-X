#ifndef BONGO_CAT_I18N_H
#define BONGO_CAT_I18N_H

#include "bongo_cat/config.h"

typedef struct BongoCatI18n BongoCatI18n;

BongoCatI18n *bongo_cat_i18n_create(const char *root, BongoCatLanguage language, BongoCatError *error);
void bongo_cat_i18n_destroy(BongoCatI18n *i18n);
BongoCatResult bongo_cat_i18n_reload(BongoCatI18n *i18n, BongoCatLanguage language,
    BongoCatError *error);
const char *bongo_cat_i18n_get(const BongoCatI18n *i18n, const char *key,
    const char *fallback);
size_t bongo_cat_i18n_glyph_ranges(const BongoCatI18n *i18n, uint32_t *ranges,
    size_t capacity);
size_t bongo_cat_i18n_all_glyph_ranges(const BongoCatI18n *i18n, uint32_t *ranges,
    size_t capacity);
/* Native name of each language, shown verbatim in the settings language
   list; the all-glyph-ranges builder covers its script even when no locale
   value contains it. */
const char *bongo_cat_ui_language_name(BongoCatLanguage language);

#endif
