#include "overlay_internal.h"
#include "bongo_cat/path.h"

#include <stdio.h>
#include <string.h>

bool bongo_cat_overlay_key_path(BongoCatOverlay *value, bool right,
    const char *name, char path[BONGO_CAT_PATH_CAP]) {
    const char *group = right ? "right-keys" : "left-keys";
    char relative[BONGO_CAT_PATH_CAP];
    snprintf(relative, sizeof(relative), "resources/%s/%s.png", group, name);
    if (!bongo_cat_path_join(path, BONGO_CAT_PATH_CAP, value->directory, relative))
        return false;
    if (bongo_cat_path_is_file(path)) return true;
    if (name[0] == 'F' && name[1] >= '0' && name[1] <= '9') {
        snprintf(relative, sizeof(relative), "resources/%s/Fn.png", group);
        return bongo_cat_path_join(path, BONGO_CAT_PATH_CAP, value->directory,
            relative) && bongo_cat_path_is_file(path);
    }
    return false;
}

int bongo_cat_overlay_key(BongoCatOverlay *value, const char *name, bool pressed) {
    if (!value || !name) return -1;
    ++value->native_input_revision;
    if (!pressed && bongo_cat_overlay_keys_enabled(value)) {
        if (bongo_cat_overlay_keys_release(value, true, name)) return 1;
        if (bongo_cat_overlay_keys_release(value, false, name)) return 0;
    }
    bool right;
    char path[BONGO_CAT_PATH_CAP];
    if (bongo_cat_overlay_key_path(value, true, name, path)) right = true;
    else if (bongo_cat_overlay_key_path(value, false, name, path)) right = false;
    else return -1;
    if (bongo_cat_overlay_keys_enabled(value)) {
        if (pressed) {
            snprintf(value->last_input_path, sizeof(value->last_input_path), "%s", path);
            bongo_cat_overlay_keys_press(value, right, name, path);
        } else bongo_cat_overlay_keys_release(value, right, name);
        return right ? 1 : 0;
    }
    char *active_name = right ? value->right_name : value->left_name;
    GLuint *active = right ? &value->right : &value->left;
    char *active_path = right ? value->right_path : value->left_path;
    if (pressed) {
        snprintf(value->last_input_path, sizeof(value->last_input_path), "%s", path);
        snprintf(active_name, BONGO_CAT_ID_CAP, "%s", name);
#ifdef BONGO_CAT_HAS_MODEL_PLUGINS
        snprintf(active_path, BONGO_CAT_PATH_CAP, "%s", path);
        *active = value->composed_cover ? 1 : bongo_cat_overlay_cached_texture(value, path);
        if (!*active) value->input_texture_failures++;
#else
        snprintf(active_path, BONGO_CAT_PATH_CAP, "%s", path);
        *active = 1;
#endif
    } else if (strcmp(active_name, name) == 0) {
        active_name[0] = '\0';
        *active = 0;
        active_path[0] = '\0';
    }
    if (value->composed_cover) value->composite_dirty = true;
    return right ? 1 : 0;
}

bool bongo_cat_overlay_hand_active(const BongoCatOverlay *value, bool right) {
    if (!value) return false;
    if (bongo_cat_overlay_keys_enabled(value))
        return (right ? value->right_keys.held_count : value->left_keys.held_count) != 0;
    return (right ? value->right : value->left) != 0;
}

BongoCatOverlayInputDiagnostics bongo_cat_overlay_input_diagnostics(
    const BongoCatOverlay *value) {
    if (!value) return (BongoCatOverlayInputDiagnostics){"", "", "", 0, 0};
    return (BongoCatOverlayInputDiagnostics){value->directory,
        value->last_input_path, value->effect_path,
        (value->left || value->left_keys.count ? 1u : 0u) |
            (value->right || value->right_keys.count ? 2u : 0u),
        value->input_texture_failures};
}

bool bongo_cat_overlay_effect(BongoCatOverlay *value, const char *path) {
    if (!value) return false;
    ++value->native_input_revision;
    value->effect = 0;
    value->effect_path[0] = '\0';
    if (!path || !*path) return true;
    if (!bongo_cat_path_is_file(path)) return false;
#ifdef BONGO_CAT_HAS_MODEL_PLUGINS
    value->effect = bongo_cat_overlay_cached_texture(value, path);
#else
    value->effect = 1;
#endif
    snprintf(value->effect_path, sizeof(value->effect_path), "%s", path);
    return value->effect != 0;
}
