#include "bongo_cat/json_dom.h"
#include "bongo_cat/json.h"
#include <stdlib.h>

/* File access remains in the host CRT. Parsing and serialization stay in Rust. */
BongoJsonDoc *bongo_json_read_fp(FILE *file, BongoJsonReadFlags flags,
    const void *allocator, BongoJsonReadError *error) {
    if (!file || allocator) return NULL;
    const size_t limit = 16u * 1024u * 1024u;
    size_t capacity = 4096, length = 0;
    char *data = malloc(capacity);
    if (!data) return NULL;
    for (;;) {
        size_t read = fread(data + length, 1, capacity - length, file);
        length += read;
        if (ferror(file)) { free(data); return NULL; }
        if (feof(file)) break;
        if (length == capacity) {
            if (capacity >= limit) { free(data); return NULL; }
            size_t next = capacity * 2;
            char *larger = realloc(data, next);
            if (!larger) { free(data); return NULL; }
            data = larger; capacity = next;
        }
    }
    BongoJsonDoc *doc = bongo_json_read_opts(data, length, flags, NULL, error);
    free(data);
    return doc;
}
BongoJsonDoc *bongo_json_read_file(const char *path, BongoJsonReadFlags flags,
    const void *allocator, BongoJsonReadError *error) {
    return allocator ? NULL : bongo_cat_json_read_file(path, flags, error);
}
bool bongo_json_mut_write_fp(FILE *file, const BongoJsonMutDoc *doc,
    BongoJsonWriteFlags flags, const void *allocator, BongoJsonWriteError *error) {
    if (!file || !doc || allocator) return false;
    if (error) *error = (BongoJsonWriteError){0};
    size_t length = 0;
    char *text = bongo_json_write(doc, flags, &length);
    bool result = text && fwrite(text, 1, length, file) == length;
    bongo_json_free_text(text);
    if (!result && error) error->code = 1;
    return result;
}
