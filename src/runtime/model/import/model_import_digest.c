#include "model_import.h"
#include "model_import_digest_internal.h"
#include "model_import_mver_policy.h"
#include "bongo_cat/path.h"
#include "bongo_cat/sha256.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define DIGEST_ROOT_CAP 12
#define path_length bongo_cat_import_digest_path_length
#define path_contains bongo_cat_import_digest_path_contains
#define hash_text bongo_cat_import_digest_hash_text
#define mix bongo_cat_import_digest_mix

typedef struct DigestStamp { uint64_t sum, exclusive, bytes, files, entries; } DigestStamp;

typedef struct DigestWalk {
    const char *root;
    const char *group;
    DigestStamp *stamp;
    const char *content_root;
    DigestStamp *content;
    const char *override_root;
    DigestStamp *overrides;
    BongoCatImportDigestCache *cache;
    int depth;
} DigestWalk;

typedef struct DigestRoot { char path[BONGO_CAT_PATH_CAP]; const char *group; } DigestRoot;

static bool stamp_path(const char *path, DigestWalk *walk);

static BongoCatPathVisit stamp_item(void *userdata,
    const char *dirname, const char *name) {
    DigestWalk *walk = userdata;
    char path[BONGO_CAT_PATH_CAP];
    return ++walk->stamp->entries <= 16384 &&
        bongo_cat_path_join(path, sizeof(path), dirname, name) &&
        stamp_path(path, walk) ? BONGO_CAT_PATH_CONTINUE :
        BONGO_CAT_PATH_FAILURE;
}

static const char *relative_path(const char *root, const char *path) {
    size_t root_length = path_length(root);
    if (!path_contains(root, path)) return path;
    const char *relative = path + root_length;
    while (*relative == '/' || *relative == '\\') relative++;
    return relative;
}

static bool stamp_path(const char *path, DigestWalk *walk) {
    if (bongo_cat_path_is_dir(path)) {
        if (walk->depth >= 24) return false;
        walk->depth++;
        bool ok = bongo_cat_path_enumerate(path, stamp_item, walk);
        walk->depth--;
        return ok;
    }
    uint64_t size = 0, modified = 0;
    if (!bongo_cat_path_file_info(path, &size, &modified) ||
        walk->stamp->files >= 8192 ||
        size > 1073741824ull - walk->stamp->bytes) return false;
    char file_hash[65];
    if (!bongo_cat_import_digest_file_cached(walk->cache, path, size,
        modified, file_hash))
        return false;
    uint64_t value = mix(hash_text(walk->group) ^
        hash_text(relative_path(walk->root, path)) ^ hash_text(file_hash) ^
        mix(size));
    walk->stamp->sum += value;
    walk->stamp->exclusive ^= value;
    walk->stamp->bytes += size;
    walk->stamp->files++;
    if (walk->content && path_contains(walk->content_root, path)) {
        value = mix(hash_text("content") ^
            hash_text(relative_path(walk->content_root, path)) ^
            hash_text(file_hash) ^ mix(size));
        walk->content->sum += value;
        walk->content->exclusive ^= value;
        walk->content->bytes += size;
        walk->content->files++;
    }
    if (walk->overrides && path_contains(walk->override_root, path)) {
        value = mix(hash_text("overrides") ^
            hash_text(relative_path(walk->override_root, path)) ^
            hash_text(file_hash) ^ mix(size));
        walk->overrides->sum += value;
        walk->overrides->exclusive ^= value;
        walk->overrides->bytes += size;
        walk->overrides->files++;
    }
    return true;
}

static bool stamp_root(const char *root, const char *group,
    DigestStamp *stamp, const BongoCatImportCandidate *candidate,
    DigestStamp *content, DigestStamp *overrides,
    BongoCatImportDigestCache *cache) {
    if (!root || !root[0]) return true;
    DigestWalk walk = {root, group, stamp,
        candidate->assets, content, candidate->overrides, overrides, cache, 0};
    return stamp_path(root, &walk);
}

static void add_root(DigestRoot roots[DIGEST_ROOT_CAP], size_t *count,
    const char *path, const char *group) {
    if (!path || !path[0]) return;
    for (size_t i = 0; i < *count; ++i)
        if (path_contains(roots[i].path, path)) return;
    bool merged = false;
    for (size_t i = 0; i < *count;) {
        if (!path_contains(path, roots[i].path)) { i++; continue; }
        roots[i] = roots[--*count];
        merged = true;
    }
    if (merged) group = "content";
    if (*count < DIGEST_ROOT_CAP) {
        DigestRoot *root = &roots[(*count)++];
        snprintf(root->path, sizeof(root->path), "%s", path);
        root->group = group;
    }
}

static bool roots_contain(const DigestRoot *roots, size_t count,
    const char *path) {
    for (size_t i = 0; i < count; ++i)
        if (path_contains(roots[i].path, path)) return true;
    return false;
}

static bool first_file(const char *directory, const char *const *names,
    size_t name_count, char *path, size_t capacity, const char **name) {
    for (size_t i = 0; i < name_count; ++i)
        if (bongo_cat_path_join(path, capacity, directory, names[i]) &&
            bongo_cat_path_is_file(path)) {
            if (name) *name = names[i];
            return true;
        }
    return false;
}

static void add_tauri_preview_roots(const BongoCatImportCandidate *candidate,
    DigestRoot roots[DIGEST_ROOT_CAP], size_t *count) {
    static const char *const covers[] = {
        "cover.png", "cat.png", "bg.png", "mousebg.png", "tabletbg.png"
    };
    static const char *const backgrounds[] = {
        "background.png", "bg.png", "mousebg.png", "tabletbg.png"
    };
    char resources[BONGO_CAT_PATH_CAP], path[BONGO_CAT_PATH_CAP];
    bool has_resources = bongo_cat_path_join(resources, sizeof(resources),
        candidate->assets, "resources") && bongo_cat_path_is_dir(resources);
    if (has_resources)
        add_root(roots, count, resources, "preview-resources");
    const char *name = NULL;
    bool resource_cover = has_resources && first_file(resources, covers,
        sizeof(covers) / sizeof(covers[0]), path, sizeof(path), NULL);
    if (!resource_cover && first_file(candidate->assets, covers,
            sizeof(covers) / sizeof(covers[0]), path, sizeof(path), &name))
        add_root(roots, count, path, name);
    bool resource_background = has_resources && first_file(resources,
        backgrounds, sizeof(backgrounds) / sizeof(backgrounds[0]), path,
        sizeof(path), NULL);
    if (!resource_background && first_file(candidate->assets, backgrounds,
            sizeof(backgrounds) / sizeof(backgrounds[0]), path,
            sizeof(path), &name))
        add_root(roots, count, path, name);
    if (bongo_cat_path_join(path, sizeof(path), candidate->assets,
            "cover.png") && bongo_cat_path_is_file(path))
        add_root(roots, count, path, "authored-cover");
}

static bool stamp_equal(const DigestStamp *value, uint64_t sum, uint64_t exclusive,
    uint64_t bytes, uint64_t files) {
    return value->sum == sum && value->exclusive == exclusive &&
        value->bytes == bytes && value->files == files;
}

static bool candidate_placeholder(const BongoCatImportCandidate *candidate,
    const DigestStamp *overrides,
    BongoCatImportDigestCache *cache) {
    /* Identify bundled runtime templates by their rendered Live2D core. Do
       not use aggregate asset stamps here: older releases shipped a popular
       authored skin with the same file counts and byte totals. */
    if (candidate->format == BONGO_CAT_IMPORT_TAURI ||
        candidate->format == BONGO_CAT_IMPORT_INOCHI2D) return false;
    if (bongo_cat_import_mver_stock_model(candidate, cache)) return true;
    if (candidate->format == BONGO_CAT_IMPORT_MVER_PATCH) {
        if (!overrides->files) return true;
        if (candidate->mode != BONGO_CAT_MODE_GAMEPAD) return false;
        return stamp_equal(overrides, 0x3830b3ef1cf413b7ull,
                0xf36e3216ab32c519ull, 828360ull, 12ull) ||
            stamp_equal(overrides, 0xe35c6e634128d62bull,
                0x17e2f6d0ad5746bfull, 1297880ull, 12ull);
    }
    return false;
}

bool bongo_cat_import_candidate_inspect_cached(
    const BongoCatImportCandidate *candidate, char output[65],
    bool *placeholder, BongoCatImportDigestCache *cache,
    BongoCatError *error) {
    if (!candidate || !output) return false;
    char setting[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_path_join(setting, sizeof(setting),
            candidate->directory, candidate->setting) ||
        !bongo_cat_path_is_file(setting)) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
            "Imported model manifest is missing: %s", candidate->setting);
        return false;
    }
    DigestRoot roots[DIGEST_ROOT_CAP] = {0};
    size_t root_count = 0;
    if (candidate->format == BONGO_CAT_IMPORT_INOCHI2D) {
        add_root(roots, &root_count, setting, "model");
        static const char *const extra[] = { "bongocat.bindings.json", "cover.png", "background.png", "resources" };
        for (size_t i = 0; i < sizeof(extra) / sizeof(extra[0]); ++i) {
            char path[BONGO_CAT_PATH_CAP];
            if (bongo_cat_path_join(path, sizeof(path), candidate->directory, extra[i]) &&
                (bongo_cat_path_is_file(path) || bongo_cat_path_is_dir(path)))
                add_root(roots, &root_count, path, extra[i]);
        }
    } else add_root(roots, &root_count, candidate->directory, "model");
    if (candidate->format == BONGO_CAT_IMPORT_TAURI)
        add_tauri_preview_roots(candidate, roots, &root_count);
    else if (candidate->format != BONGO_CAT_IMPORT_INOCHI2D) {
        add_root(roots, &root_count, candidate->assets, "assets");
        add_root(roots, &root_count, candidate->overrides, "overrides");
    }
    if (candidate->config[0] &&
        !roots_contain(roots, root_count, candidate->config))
        add_root(roots, &root_count, candidate->config, "configuration");
    DigestStamp stamp = {0}, content = {0}, overrides = {0};
    for (size_t i = 0; i < root_count; ++i) {
        if (stamp_root(roots[i].path, roots[i].group, &stamp, candidate,
                &content, &overrides, cache)) continue;
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
            "Cannot fingerprint imported model assets: %s",
            candidate->package_root);
        return false;
    }
    char summary[512];
    int length = snprintf(summary, sizeof(summary),
        "%016llx|%s|%d|%d|%016llx|%016llx|%llu|%llu",
        (unsigned long long)hash_text(candidate->setting),
        bongo_cat_mode_name(candidate->mode), (int)candidate->format,
        candidate->gamepad_buttons ? 1 : 0,
        (unsigned long long)stamp.sum,
        (unsigned long long)stamp.exclusive,
        (unsigned long long)stamp.bytes,
        (unsigned long long)stamp.files);
    if (length < 0 || (size_t)length >= sizeof(summary)) return false;
    bongo_cat_sha256_bytes(summary, (size_t)length, output);
    if (placeholder)
        *placeholder = candidate_placeholder(candidate, &overrides, cache);
    return true;
}
