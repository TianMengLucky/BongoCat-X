#ifndef BONGO_CAT_JSON_H
#define BONGO_CAT_JSON_H

#include "bongo_cat/common.h"
#include "bongo_cat/json_dom.h"

#ifdef __cplusplus
extern "C" {
#endif

BongoJsonDoc *bongo_cat_json_read_file(const char *path,
    BongoJsonReadFlags flags, BongoJsonReadError *error);
/* Model compatibility is applied in memory; authored files are never rewritten. */
BongoJsonDoc *bongo_cat_model_json_parse(const char *data, size_t size,
    bool *normalized);
BongoJsonDoc *bongo_cat_model_json_read(const char *path, bool *normalized);
bool bongo_cat_json_write_file(const char *path,
    const BongoJsonMutDoc *document, BongoJsonWriteFlags flags,
    BongoJsonWriteError *error);

#ifdef __cplusplus
}
#endif

#endif
