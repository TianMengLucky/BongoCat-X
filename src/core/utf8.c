#include "bongo_cat/utf8.h"
#include "bongo_cat/safe_core.h"
bool bongo_cat_utf8_valid(const char *text) {
    return bongo_safe_utf8_valid(text);
}
bool bongo_cat_utf8_normalize_mver(const char *text, char *output, size_t capacity) {
    return bongo_safe_utf8_normalize(text, output, capacity);
}
