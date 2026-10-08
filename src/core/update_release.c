#include "bongo_cat/update.h"

#include <stdio.h>
#include <string.h>
#include "bongo_cat/json_dom.h"

#define RELEASE_PREFIX \
    "https://github.com/TianMengLucky/BongoCat-X/releases/tag/"
#define DOWNLOAD_PREFIX \
    "https://github.com/TianMengLucky/BongoCat-X/releases/download/"

static bool copy_text(char *target, size_t capacity, BongoJsonValue *value,
    bool required, bool truncate) {
    if (!value || bongo_json_is_null(value)) return !required;
    if (!bongo_json_is_str(value)) return false;
    const char *text = bongo_json_get_str(value);
    size_t length = bongo_json_get_len(value);
    if (!text || strlen(text) != length || (required && !length)) return false;
    if (length >= capacity) {
        if (!truncate) return false;
        length = capacity - 1;
    }
    memcpy(target, text, length);
    target[length] = '\0';
    return true;
}

static bool safe_url(const char *url, const char *prefix) {
    if (!url || strncmp(url, prefix, strlen(prefix)) != 0) return false;
    for (const unsigned char *at = (const unsigned char *)url; *at; ++at)
        if (*at <= 0x20 || *at == '\\') return false;
    return true;
}

static bool copy_asset_url(BongoJsonValue *asset, const char *expected,
    char *target, size_t capacity) {
    BongoJsonValue *name_value = bongo_json_obj_get(asset, "name");
    BongoJsonValue *url_value = bongo_json_obj_get(asset, "browser_download_url");
    if (!bongo_json_is_str(name_value) || !bongo_json_is_str(url_value) ||
        strcmp(bongo_json_get_str(name_value), expected) != 0) return false;
    const char *url = bongo_json_get_str(url_value);
    const char *filename = url ? strrchr(url, '/') : NULL;
    if (!safe_url(url, DOWNLOAD_PREFIX) || !filename ||
        strcmp(filename + 1, expected) != 0 || strlen(url) >= capacity)
        return false;
    snprintf(target, capacity, "%s", url);
    return true;
}

static bool read_assets(BongoJsonValue *root, const char *platform,
    BongoCatUpdateRelease *release) {
    BongoJsonValue *assets = bongo_json_obj_get(root, "assets");
    if (!bongo_json_is_arr(assets)) return false;
    char installer[128] = {0}, portable[128] = {0};
    int installer_length = 0, portable_length = 0;
    if (strncmp(platform, "windows-", 8) == 0) {
        installer_length = snprintf(installer, sizeof(installer),
            "BongoCat-%s-%s-setup.exe", release->version, platform);
        portable_length = snprintf(portable, sizeof(portable),
            "BongoCat-%s-%s-portable.zip", release->version, platform);
    } else if (strcmp(platform, "linux-x64") == 0) {
        portable_length = snprintf(portable, sizeof(portable),
            "BongoCat-%s-%s.tar.gz", release->version, platform);
    } else if (strcmp(platform, "macos-x64") == 0 ||
        strcmp(platform, "macos-arm64") == 0) {
        portable_length = snprintf(portable, sizeof(portable),
            "BongoCat-%s-%s.zip", release->version, platform);
    } else return false;
    if (installer_length < 0 || (size_t)installer_length >= sizeof(installer) ||
        portable_length < 0 || (size_t)portable_length >= sizeof(portable))
        return false;
    size_t index, count;
    BongoJsonValue *asset;
    bongo_json_arr_foreach(assets, index, count, asset) {
        if (!bongo_json_is_obj(asset)) continue;
        if (installer[0] && !release->installer_url[0])
            copy_asset_url(asset, installer, release->installer_url,
                sizeof(release->installer_url));
        if (!release->portable_url[0]) copy_asset_url(asset, portable,
            release->portable_url, sizeof(release->portable_url));
    }
    return true;
}

bool bongo_cat_update_parse_release(const char *json, const char *platform,
    BongoCatUpdateRelease *release, BongoCatError *error) {
    if (!json || !platform || !release) return false;
    memset(release, 0, sizeof(*release));
    BongoJsonDoc *document = bongo_json_read(json, strlen(json), 0);
    BongoJsonValue *root = document ? bongo_json_doc_get_root(document) : NULL;
    BongoJsonValue *draft = bongo_json_is_obj(root)
        ? bongo_json_obj_get(root, "draft") : NULL;
    BongoJsonValue *prerelease = bongo_json_is_obj(root)
        ? bongo_json_obj_get(root, "prerelease") : NULL;
    bool valid = bongo_json_is_obj(root) && bongo_json_is_bool(draft) &&
        bongo_json_is_bool(prerelease) && !bongo_json_get_bool(draft) &&
        !bongo_json_get_bool(prerelease);
    char tag[BONGO_CAT_UPDATE_VERSION_CAP + 1] = {0};
    if (valid) valid = copy_text(tag, sizeof(tag),
        bongo_json_obj_get(root, "tag_name"), true, false);
    const char *version = tag[0] == 'v' || tag[0] == 'V' ? tag + 1 : tag;
    if (valid) valid = bongo_cat_update_version_valid(version);
    if (valid) snprintf(release->version, sizeof(release->version), "%s",
        version);
    if (valid) valid = copy_text(release->release_url,
        sizeof(release->release_url), bongo_json_obj_get(root, "html_url"), true,
        false) &&
        safe_url(release->release_url, RELEASE_PREFIX);
    if (valid) {
        BongoJsonValue *body = bongo_json_obj_get(root, "body");
        if (body && !copy_text(release->notes, sizeof(release->notes),
                body, false, true)) valid = false;
    }
    if (valid) valid = read_assets(root, platform, release);
    bongo_json_doc_free(document);
    if (!valid) bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
        "GitHub returned invalid BongoCat release metadata");
    return valid;
}
