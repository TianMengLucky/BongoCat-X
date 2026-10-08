#ifndef BONGO_CAT_JSON_DOM_H
#define BONGO_CAT_JSON_DOM_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
/* Rust owns all nodes and strings. Borrowed handles expire on document free.
   Mutations copy strings. No allocator or FILE handle crosses the Rust ABI. */
typedef struct BongoJsonDoc BongoJsonDoc;
typedef struct BongoJsonValue BongoJsonValue;
typedef BongoJsonDoc BongoJsonMutDoc;
typedef BongoJsonValue BongoJsonMutValue;
typedef uint32_t BongoJsonReadFlags;
typedef uint32_t BongoJsonWriteFlags;
typedef struct BongoJsonReadError { int code; size_t pos; char msg[256]; } BongoJsonReadError;
typedef struct BongoJsonWriteError { int code; char msg[256]; } BongoJsonWriteError;
#define BONGO_JSON_READ_JSON5 1u
#define BONGO_JSON_READ_ALLOW_INVALID_UNICODE 2u
#define BONGO_JSON_READ_ALLOW_COMMENTS 4u
#define BONGO_JSON_READ_ALLOW_TRAILING_COMMAS 8u
#define BONGO_JSON_READ_ALLOW_BOM 16u
#define BONGO_JSON_WRITE_PRETTY 1u
#ifdef __cplusplus
extern "C" {
#endif
BongoJsonDoc *bongo_json_read(const char *data, size_t size, BongoJsonReadFlags flags);
BongoJsonDoc *bongo_json_read_opts(const char *data, size_t size, BongoJsonReadFlags flags,
    const void *allocator, BongoJsonReadError *error);
BongoJsonDoc *bongo_json_read_fp(FILE *file, BongoJsonReadFlags flags,
    const void *allocator, BongoJsonReadError *error);
BongoJsonDoc *bongo_json_read_file(const char *path, BongoJsonReadFlags flags,
    const void *allocator, BongoJsonReadError *error);
void bongo_json_doc_free(BongoJsonDoc *doc);
BongoJsonValue *bongo_json_doc_get_root(const BongoJsonDoc *doc);
BongoJsonDoc *bongo_safe_model_json_parse(const char *data, size_t size, bool *normalized);
char *bongo_json_write(const BongoJsonDoc *doc, BongoJsonWriteFlags flags, size_t *length);
void bongo_json_free_text(char *text);
BongoJsonMutDoc *bongo_json_mut_doc_new(const void *allocator);
void bongo_json_mut_doc_free(BongoJsonMutDoc *doc);
void bongo_json_mut_doc_set_root(BongoJsonMutDoc *doc, BongoJsonMutValue *value);
BongoJsonMutValue *bongo_json_mut_obj(BongoJsonMutDoc *doc);
BongoJsonMutValue *bongo_json_mut_arr(BongoJsonMutDoc *doc);
bool bongo_json_mut_write_fp(FILE *file, const BongoJsonMutDoc *doc,
    BongoJsonWriteFlags flags, const void *allocator, BongoJsonWriteError *error);
bool bongo_json_is_null(const BongoJsonValue *value);
bool bongo_json_is_bool(const BongoJsonValue *value);
bool bongo_json_is_num(const BongoJsonValue *value);
bool bongo_json_is_str(const BongoJsonValue *value);
bool bongo_json_is_arr(const BongoJsonValue *value);
bool bongo_json_is_obj(const BongoJsonValue *value);
bool bongo_json_is_int(const BongoJsonValue *value);
bool bongo_json_is_uint(const BongoJsonValue *value);
bool bongo_json_get_bool(const BongoJsonValue *value);
bool bongo_json_is_true(const BongoJsonValue *value);
double bongo_json_get_num(const BongoJsonValue *value);
int64_t bongo_json_get_int(const BongoJsonValue *value);
int64_t bongo_json_get_sint(const BongoJsonValue *value);
const char * bongo_json_get_str(const BongoJsonValue *value);
const char * bongo_json_mut_get_str(const BongoJsonValue *value);
size_t bongo_json_get_len(const BongoJsonValue *value);
uint32_t bongo_json_get_type(const BongoJsonValue *value);
size_t bongo_json_arr_size(const BongoJsonValue *value);
size_t bongo_json_obj_size(const BongoJsonValue *value);
size_t bongo_json_mut_arr_size(const BongoJsonValue *value);
BongoJsonValue *bongo_json_arr_get_first(const BongoJsonValue *arr);
BongoJsonValue *bongo_json_arr_get(const BongoJsonValue *value, size_t index);
BongoJsonValue *bongo_json_mut_arr_get(const BongoJsonValue *value, size_t index);
BongoJsonValue *bongo_json_obj_key_at(const BongoJsonValue *value, size_t index);
BongoJsonValue *bongo_json_obj_value_at(const BongoJsonValue *value, size_t index);
BongoJsonValue *bongo_json_obj_get(const BongoJsonValue *obj, const char *key);
BongoJsonValue *bongo_json_mut_obj_get(const BongoJsonValue *obj, const char *key);
bool bongo_json_mut_obj_add_bool(BongoJsonMutDoc *doc, BongoJsonMutValue *obj, const char *key, bool value);
bool bongo_json_mut_obj_add_int(BongoJsonMutDoc *doc, BongoJsonMutValue *obj, const char *key, int64_t value);
bool bongo_json_mut_obj_add_uint(BongoJsonMutDoc *doc, BongoJsonMutValue *obj, const char *key, uint64_t value);
bool bongo_json_mut_obj_add_real(BongoJsonMutDoc *doc, BongoJsonMutValue *obj, const char *key, double value);
bool bongo_json_mut_obj_add_str(BongoJsonMutDoc *doc, BongoJsonMutValue *obj, const char *key, const char * value);
bool bongo_json_mut_obj_add_strcpy(BongoJsonMutDoc *doc, BongoJsonMutValue *obj, const char *key, const char * value);
BongoJsonMutValue *bongo_json_mut_obj_add_obj(BongoJsonMutDoc *doc, BongoJsonMutValue *obj, const char *key);
BongoJsonMutValue *bongo_json_mut_arr_add_obj(BongoJsonMutDoc *doc, BongoJsonMutValue *arr);
BongoJsonMutValue *bongo_json_mut_obj_add_arr(BongoJsonMutDoc *doc, BongoJsonMutValue *obj, const char *key);
BongoJsonMutValue *bongo_json_mut_arr_add_arr(BongoJsonMutDoc *doc, BongoJsonMutValue *arr);
bool bongo_json_mut_obj_remove_key(BongoJsonMutValue *obj, const char *key);
bool bongo_json_mut_arr_add_val(BongoJsonMutValue *arr, BongoJsonMutValue *value);
bool bongo_json_mut_arr_add_int(BongoJsonMutDoc *doc, BongoJsonMutValue *arr, int64_t value);
bool bongo_json_mut_arr_add_real(BongoJsonMutDoc *doc, BongoJsonMutValue *arr, double value);
#ifdef __cplusplus
}
#endif
#define bongo_json_arr_foreach(arr, index, count, value) \
    for ((index)=0, (count)=bongo_json_arr_size(arr); \
        (index)<(count) && (((value)=bongo_json_arr_get((arr),(index))),true); ++(index))
#define bongo_json_mut_arr_foreach bongo_json_arr_foreach
#define bongo_json_obj_foreach(obj, index, count, key, value) \
    for ((index)=0, (count)=bongo_json_obj_size(obj); \
        (index)<(count) && (((key)=bongo_json_obj_key_at((obj),(index))), \
        ((value)=bongo_json_obj_value_at((obj),(index))),true); ++(index))
typedef struct BongoJsonArrIter { BongoJsonValue *array; size_t index; } BongoJsonArrIter;
static inline BongoJsonArrIter bongo_json_arr_iter_with(BongoJsonValue *array) {
    BongoJsonArrIter iter = {array,0}; return iter;
}
static inline BongoJsonValue *bongo_json_arr_iter_next(BongoJsonArrIter *iter) {
    return iter ? bongo_json_arr_get(iter->array, iter->index++) : NULL;
}
#endif
