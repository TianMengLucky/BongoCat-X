#include "bongo_cat/json.h"
#include "bongo_cat/file.h"
#include "bongo_cat/path.h"

#include <stdio.h>

#define BONGO_CAT_JSON_FILE_CAP (16u * 1024u * 1024u)

BongoJsonDoc *bongo_cat_json_read_file(const char *path,
    BongoJsonReadFlags flags, BongoJsonReadError *error) {
    uint64_t size;
    if (!bongo_cat_path_file_size(path, &size) ||
        size == 0 || size > BONGO_CAT_JSON_FILE_CAP) return NULL;
    FILE *file = bongo_cat_file_open(path, "rb");
    if (!file) return NULL;
    BongoJsonDoc *document = bongo_json_read_fp(file, flags, NULL, error);
    if (fclose(file) != 0 && document) {
        bongo_json_doc_free(document); document = NULL;
    }
    return document;
}

bool bongo_cat_json_write_file(const char *path,
    const BongoJsonMutDoc *document, BongoJsonWriteFlags flags,
    BongoJsonWriteError *error) {
    if (!path || !document) return false;
    FILE *file = bongo_cat_file_open(path, "wb");
    if (!file) return false;
    bool ok = bongo_json_mut_write_fp(file, document, flags, NULL, error);
    if (fclose(file) != 0) ok = false;
    if (!ok) bongo_cat_path_remove(path);
    return ok;
}
