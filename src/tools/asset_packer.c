#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "bongo_cat/json_dom.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Asset {
    char *path;
    unsigned char *data;
    size_t size;
    size_t unique;
    uint64_t hash;
    uint64_t offset;
} Asset;

typedef struct AssetList {
    Asset *items;
    size_t count;
    size_t capacity;
} AssetList;

static void free_assets(AssetList *list) {
    size_t index;
    for (index = 0; index < list->count; ++index) {
        free(list->items[index].path);
        free(list->items[index].data);
    }
    free(list->items);
    list->items = NULL;
    list->count = list->capacity = 0;
}

static int reserve_assets(AssetList *list) {
    Asset *items;
    size_t capacity = list->capacity ? list->capacity * 2 : 64;
    if (capacity < list->capacity || capacity > SIZE_MAX / sizeof(*items)) return 0;
    items = (Asset *)realloc(list->items, capacity * sizeof(*items));
    if (!items) return 0;
    list->items = items;
    list->capacity = capacity;
    return 1;
}

static char *utf8_from_wide(const wchar_t *value) {
    int length;
    char *result;
    if (!value) return NULL;
    length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1,
        NULL, 0, NULL, NULL);
    if (length <= 0) return NULL;
    result = (char *)malloc((size_t)length);
    if (!result) return NULL;
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1,
        result, length, NULL, NULL)) {
        free(result);
        return NULL;
    }
    return result;
}

static int join_wide(const wchar_t *directory, const wchar_t *name,
    wchar_t *result, size_t capacity) {
    int written = _snwprintf_s(result, capacity, _TRUNCATE, L"%ls\\%ls",
        directory, name);
    return written >= 0;
}

static int read_file(const wchar_t *path, unsigned char **data, size_t *size) {
    HANDLE handle = INVALID_HANDLE_VALUE;
    LARGE_INTEGER length;
    DWORD read_count;
    unsigned char *buffer = NULL;
    int ok = 0;
    handle = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE |
        FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == INVALID_HANDLE_VALUE || !GetFileSizeEx(handle, &length) ||
        length.QuadPart < 0 || (uint64_t)length.QuadPart > UINT32_MAX) goto done;
    if (length.QuadPart != 0) {
        buffer = (unsigned char *)malloc((size_t)length.QuadPart);
        if (!buffer) goto done;
        if (!ReadFile(handle, buffer, (DWORD)length.QuadPart, &read_count, NULL) ||
            read_count != (DWORD)length.QuadPart) goto done;
    }
    *data = buffer;
    *size = (size_t)length.QuadPart;
    buffer = NULL;
    ok = 1;
done:
    free(buffer);
    if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
    return ok;
}

/* Only locale dictionaries are normalized: model JSON and binary assets
   remain byte-identical. Parsing stays in the existing Rust JSON library. */
static int compact_locale(const char *relative, unsigned char **data, size_t *size) {
    size_t length = strlen(relative);
    if (strncmp(relative, "locales/", 8) != 0 || length < 5 ||
        strcmp(relative + length - 5, ".json") != 0) return 1;
    BongoJsonDoc *doc = bongo_json_read((const char *)*data, *size,
        BONGO_JSON_READ_ALLOW_BOM);
    if (!doc) return 0;
    size_t compact_size = 0;
    char *text = bongo_json_write(doc, 0, &compact_size);
    bongo_json_doc_free(doc);
    if (!text) return 0;
    if (compact_size < *size) {
        unsigned char *compact = malloc(compact_size ? compact_size : 1);
        if (!compact) { bongo_json_free_text(text); return 0; }
        memcpy(compact, text, compact_size);
        free(*data);
        *data = compact;
        *size = compact_size;
    }
    bongo_json_free_text(text);
    return 1;
}

static int append_asset(AssetList *list, const wchar_t *file,
    const char *relative) {
    Asset *asset;
    char *path;
    unsigned char *data = NULL;
    size_t size = 0;
    size_t relative_length = strlen(relative);
    size_t path_length = 7 + relative_length;
    if (path_length > UINT16_MAX || !read_file(file, &data, &size)) return 0;
    if (!compact_locale(relative, &data, &size)) { free(data); return 0; }
    path = (char *)malloc(path_length + 1);
    if (!path) { free(data); return 0; }
    memcpy(path, "assets/", 7);
    memcpy(path + 7, relative, relative_length + 1);
    if (list->count == list->capacity && !reserve_assets(list)) {
        free(path); free(data); return 0;
    }
    asset = &list->items[list->count++];
    asset->path = path;
    asset->data = data;
    asset->size = size;
    asset->unique = 0;
    asset->hash = 0;
    asset->offset = 0;
    return 1;
}

static int collect_directory(const wchar_t *directory, const char *prefix,
    AssetList *list) {
    WIN32_FIND_DATAW entry;
    HANDLE find;
    wchar_t pattern[32768];
    if (!join_wide(directory, L"*", pattern, sizeof(pattern) / sizeof(*pattern)))
        return 0;
    find = FindFirstFileW(pattern, &entry);
    if (find == INVALID_HANDLE_VALUE) return 0;
    do {
        wchar_t child[32768];
        char *name = NULL;
        char *relative = NULL;
        size_t prefix_length;
        size_t name_length;
        int is_directory;
        if (wcscmp(entry.cFileName, L".") == 0 ||
            wcscmp(entry.cFileName, L"..") == 0) continue;
        is_directory = (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (is_directory && (entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
            continue;
        if (!join_wide(directory, entry.cFileName, child,
            sizeof(child) / sizeof(*child))) { FindClose(find); return 0; }
        name = utf8_from_wide(entry.cFileName);
        if (!name) { FindClose(find); return 0; }
        prefix_length = prefix ? strlen(prefix) : 0;
        name_length = strlen(name);
        relative = (char *)malloc(prefix_length + (prefix_length ? 1 : 0) +
            name_length + 1);
        if (!relative) { free(name); FindClose(find); return 0; }
        if (prefix_length) {
            memcpy(relative, prefix, prefix_length);
            relative[prefix_length] = '/';
            memcpy(relative + prefix_length + 1, name, name_length + 1);
        } else memcpy(relative, name, name_length + 1);
        free(name);
        if (is_directory) {
            if (!collect_directory(child, relative, list)) {
                free(relative); FindClose(find); return 0;
            }
        } else if (!append_asset(list, child, relative)) {
            free(relative); FindClose(find); return 0;
        }
        free(relative);
    } while (FindNextFileW(find, &entry));
    FindClose(find);
    return GetLastError() == ERROR_NO_MORE_FILES;
}

static int compare_assets(const void *left, const void *right) {
    const Asset *a = (const Asset *)left;
    const Asset *b = (const Asset *)right;
    return strcmp(a->path, b->path);
}

static uint64_t hash_bytes(const unsigned char *data, size_t size) {
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t index;
    for (index = 0; index < size; ++index) {
        hash ^= data[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static int same_data(const Asset *left, const Asset *right) {
    return left->size == right->size &&
        (left->size == 0 || memcmp(left->data, right->data, left->size) == 0);
}

static int write_le16(FILE *stream, uint16_t value) {
    unsigned char bytes[2] = {(unsigned char)value, (unsigned char)(value >> 8)};
    return fwrite(bytes, 1, sizeof(bytes), stream) == sizeof(bytes);
}

static int write_le32(FILE *stream, uint32_t value) {
    unsigned char bytes[4];
    unsigned int index;
    for (index = 0; index < 4; ++index) bytes[index] =
        (unsigned char)(value >> (index * 8));
    return fwrite(bytes, 1, sizeof(bytes), stream) == sizeof(bytes);
}

static int write_le64(FILE *stream, uint64_t value) {
    unsigned char bytes[8];
    unsigned int index;
    for (index = 0; index < 8; ++index) bytes[index] =
        (unsigned char)(value >> (index * 8));
    return fwrite(bytes, 1, sizeof(bytes), stream) == sizeof(bytes);
}

static int write_pack(const wchar_t *output, AssetList *list) {
    FILE *stream;
    size_t index, prior;
    uint64_t offset = 12;
    static const unsigned char magic[8] = {'L','2','D','P','A','K','1','\0'};
    if (list->count > UINT32_MAX) return 0;
    qsort(list->items, list->count, sizeof(*list->items), compare_assets);
    for (index = 0; index < list->count; ++index)
        list->items[index].hash = hash_bytes(list->items[index].data,
            list->items[index].size);
    for (index = 0; index < list->count; ++index) {
        list->items[index].unique = index;
        for (prior = 0; prior < index; ++prior) {
            if (list->items[prior].hash == list->items[index].hash &&
                same_data(&list->items[prior], &list->items[index])) {
                list->items[index].unique = prior;
                break;
            }
        }
    }
    for (index = 0; index < list->count; ++index) offset +=
        16 + strlen(list->items[index].path);
    for (index = 0; index < list->count; ++index) {
        if (list->items[index].unique == index) {
            list->items[index].offset = offset;
            offset += list->items[index].size;
        }
    }
    for (index = 0; index < list->count; ++index)
        list->items[index].offset = list->items[list->items[index].unique].offset;
    stream = _wfopen(output, L"wb");
    if (!stream) return 0;
    if (fwrite(magic, 1, sizeof(magic), stream) != sizeof(magic) ||
        !write_le32(stream, (uint32_t)list->count)) { fclose(stream); return 0; }
    for (index = 0; index < list->count; ++index) {
        size_t length = strlen(list->items[index].path);
        if (!write_le16(stream, (uint16_t)length) || !write_le16(stream, 0) ||
            !write_le32(stream, (uint32_t)list->items[index].size) ||
            !write_le64(stream, list->items[index].offset) ||
            fwrite(list->items[index].path, 1, length, stream) != length) {
            fclose(stream); return 0;
        }
    }
    for (index = 0; index < list->count; ++index)
        if (list->items[index].unique == index && list->items[index].size != 0 &&
            fwrite(list->items[index].data, 1, list->items[index].size, stream) !=
            list->items[index].size) { fclose(stream); return 0; }
    if (fclose(stream) != 0) return 0;
    return 1;
}

int wmain(int argc, wchar_t **argv) {
    AssetList list = {0};
    int result = 1;
    if (argc != 3) {
        fwprintf(stderr, L"usage: asset_packer ROOT OUTPUT\n");
        return 2;
    }
    if (collect_directory(argv[1], "", &list) && write_pack(argv[2], &list))
        result = 0;
    else fwprintf(stderr, L"cannot create asset pack\n");
    free_assets(&list);
    return result;
}
