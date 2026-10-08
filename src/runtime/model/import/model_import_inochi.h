#ifndef BONGO_CAT_MODEL_IMPORT_INOCHI_H
#define BONGO_CAT_MODEL_IMPORT_INOCHI_H
#include "model_import.h"
int bongo_cat_import_inochi_discover(const char *source,
    BongoCatImportDiscovery *discovery, BongoCatError *error);
bool bongo_cat_import_inochi_copy(const BongoCatImportCandidate *candidate,
    const char *target, BongoCatImportCandidate *installed, BongoCatError *error);
#endif
