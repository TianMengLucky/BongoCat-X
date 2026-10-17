#include "bongo_cat/update.h"
#include "bongo_cat/safe_core.h"
int bongo_cat_update_compare_versions(const char *left, const char *right) {
    return bongo_safe_version_compare(left, right);
}
bool bongo_cat_update_version_valid(const char *version) {
    return bongo_safe_version_valid(version);
}
