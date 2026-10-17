#ifndef BONGO_CAT_MODEL_IMPORT_MVER_INTERNAL_H
#define BONGO_CAT_MODEL_IMPORT_MVER_INTERNAL_H

#include "model_import_mver.h"
#include "../../mver/mver_config_keys.h"

bool bongo_cat_mver_emit_pair(const char *hand, const char *keyboard,
    const char *directory, BongoCatMverKeyNames names, BongoCatError *error);
#endif
