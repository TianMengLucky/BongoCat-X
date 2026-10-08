#include "bongo_cat/json.h"
#include "bongo_cat/file.h"
#include "bongo_cat/path.h"

#include <stdlib.h>
#include <string.h>

BongoJsonDoc *bongo_cat_model_json_parse(const char *data, size_t size,
    bool *normalized) {
    return bongo_safe_model_json_parse(data, size, normalized);
}

BongoJsonDoc *bongo_cat_model_json_read(const char *path, bool *normalized) {
    if (normalized) *normalized = false;
    uint64_t size;
    if (!bongo_cat_path_file_size(path, &size) || !size ||
        size > 4u * 1024u * 1024u) return NULL;
    char *data = malloc((size_t)size);
    if (!data) return NULL;
    FILE *file = bongo_cat_file_open(path, "rb");
    bool loaded = file && fread(data, 1, (size_t)size, file) == (size_t)size;
    if (file && fclose(file) != 0) loaded = false;
    BongoJsonDoc *document = loaded
        ? bongo_cat_model_json_parse(data, (size_t)size, normalized) : NULL;
    free(data);
    return document;
}
