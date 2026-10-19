#include "plugin_ui_internal.h"
#include "bongo_cat/items.h"
#include "bongo_cat/sounds.h"
#include "bongo_cat/statistics.h"
#include "bongo_cat/store.h"
#include "bongo_cat/path.h"
#include "bongo_cat/platform.h"
#include <string.h>

static bool read_status(BongoCatPluginPageBinding *binding, BongoCatPluginUiStatus *out) {
    BongoCatPreferences *value = binding->preferences;
    BongoCatApp *app = value->app;
    *out = (BongoCatPluginUiStatus){0};
    switch (binding->kind) {
    case BONGO_CAT_PLUGIN_UI_SOUNDS:
        out->busy = bongo_cat_sounds_busy(app);
        out->blocked = bongo_cat_preferences_sound_picker_open(value);
        return true;
    case BONGO_CAT_PLUGIN_UI_ITEMS:
        out->busy = bongo_cat_items_busy(app);
        return true;
    case BONGO_CAT_PLUGIN_UI_STATISTICS:
        out->overlay = bongo_safe_statistics_overlay_get(app->settings.extensions_json, sizeof(app->settings.extensions_json));
        out->reset_available = bongo_cat_statistics_reset_available(app);
        out->reset_busy = bongo_cat_statistics_reset_busy(app);
        return true;
    case BONGO_CAT_PLUGIN_UI_STORE:
        out->busy = bongo_cat_store_busy(app);
        out->status = bongo_cat_store_status(app, &out->count);
        out->operation = bongo_cat_store_operation(app, &out->operation_index, &out->received, &out->total);
        return true;
    default: return false;
    }
}

uint32_t bongo_cat_preferences_plugin_read(void *context, uint32_t operation,
    uint32_t index, void *output, uint32_t size) {
    BongoCatPluginPageBinding *binding = context;
    if (!binding || !binding->preferences || !output) return 0;
    BongoCatApp *app = binding->preferences->app;
    if (operation == BONGO_UI_READ_STATUS && size == sizeof(BongoCatPluginUiStatus))
        return read_status(binding, output);
    switch (binding->kind) {
    case BONGO_CAT_PLUGIN_UI_SOUNDS:
        if (operation == BONGO_UI_READ_SNAPSHOT && size == sizeof(BongoCatSoundScene))
            return bongo_cat_sounds_snapshot(app, output);
        if (operation == BONGO_UI_READ_KEY && size == sizeof(BongoCatSoundKey))
            return bongo_cat_sounds_key(app, index, output);
        break;
    case BONGO_CAT_PLUGIN_UI_ITEMS:
        if (operation == BONGO_UI_READ_SNAPSHOT && size == sizeof(BongoCatItemScene))
            return bongo_cat_items_snapshot(app, output);
        if (operation == BONGO_UI_READ_IMAGE && size == BONGO_CAT_PATH_CAP) {
            BongoCatItemScene scene;
            char directory[BONGO_CAT_PATH_CAP];
            return bongo_cat_items_snapshot(app, &scene) && index < scene.count &&
                bongo_cat_path_join(directory, sizeof(directory), app->data_root, "items") &&
                bongo_cat_path_join(output, size, directory, scene.items[index].asset);
        }
        break;
    case BONGO_CAT_PLUGIN_UI_STATISTICS:
        if (operation == BONGO_UI_READ_BACKSPACE && size == sizeof(uint32_t))
            return bongo_cat_statistics_backspace_snapshot(app, output);
        if (operation == BONGO_UI_READ_TIMER && size == sizeof(BongoCatStatisticsTimerSnapshot))
            return bongo_cat_statistics_timer_snapshot(app, output);
        if (operation == BONGO_UI_READ_WORDS && size == sizeof(BongoCatStatisticsWordsSnapshot))
            return bongo_cat_statistics_words_snapshot(app, index, output);
        if (operation == BONGO_UI_READ_HEATMAP && size == 256 * sizeof(uint64_t))
            return bongo_cat_statistics_keyboard_snapshot(app, index, output);
        if (operation == BONGO_UI_READ_IMAGE && size == BONGO_CAT_PATH_CAP)
            return bongo_cat_statistics_usage_icon(app, index >> 16, index & 0xffffu, output);
        if (operation == BONGO_UI_READ_SECTION && size == sizeof(uint32_t)) {
            *(uint32_t *)output = binding->preferences->page == 10 ? 2u : (binding->preferences->page == 9);
            return 1;
        }
        if (operation == BONGO_UI_READ_SNAPSHOT && size == sizeof(BongoCatStatisticsSnapshot))
            return bongo_cat_statistics_snapshot(app, output);
        if (operation == BONGO_UI_READ_USAGE && size == sizeof(BongoCatApplicationSnapshot) && index < 4)
            return bongo_cat_statistics_usage_snapshot(app, index, output);
        if (operation == BONGO_UI_READ_UNITS && size == sizeof(BongoSafeStatisticsUnits))
            return bongo_safe_statistics_units_get(app->settings.extensions_json, sizeof(app->settings.extensions_json), output);
        break;
    case BONGO_CAT_PLUGIN_UI_STORE:
        if (operation == BONGO_UI_READ_ENTRY && size == sizeof(BongoCatStoreEntry))
            return bongo_cat_store_entry(app, index, output);
        if (operation == BONGO_UI_READ_REPOSITORY && size == 512)
            return bongo_safe_store_repository_get(app->settings.extensions_json,
                sizeof(app->settings.extensions_json), output, size);
        if (operation == BONGO_UI_READ_IMAGE && size == BONGO_CAT_PATH_CAP)
            return bongo_cat_store_preview(app, index, output);
        if (operation == BONGO_UI_READ_INSTALLED && size == sizeof(uint32_t)) {
            BongoCatStoreEntry entry;
            if (!bongo_cat_store_entry(app, index, &entry)) return 0;
            *(uint32_t *)output = bongo_cat_store_installed(app, &entry);
            return 1;
        }
        break;
    default: break;
    }
    return 0;
}

static bool items_action(BongoCatPreferences *value, uint32_t op,
    uint32_t index, const void *data, uint32_t size) {
    BongoCatApp *app = value->app;
    switch (op) {
    case BONGO_UI_IMPORT:
        return bongo_cat_preferences_import_items_open(value->import_dialog, value->window);
    case BONGO_UI_SAVE: return bongo_cat_items_queue_save(app);
    case BONGO_UI_OPEN_FOLDER: {
        char directory[BONGO_CAT_PATH_CAP];
        return bongo_cat_path_join(directory, sizeof(directory), app->data_root, "items") &&
            bongo_cat_path_create_directory(directory) && bongo_cat_platform_open_directory(directory);
    }
    case BONGO_UI_UPDATE_ITEM:
        return data && size == sizeof(BongoCatItem) && bongo_cat_items_update(app, index, data);
    case BONGO_UI_DUPLICATE_ITEM: return bongo_cat_items_duplicate(app, index);
    case BONGO_UI_REMOVE_ITEM: return bongo_cat_items_remove(app, index);
    default: return false;
    }
}
static bool sounds_action(BongoCatPreferences *value, uint32_t op, uint32_t index) {
    BongoCatError error = {0};
    switch (op) {
    case BONGO_UI_IMPORT: {
        BongoCatSoundKey key;
        return bongo_cat_sounds_key(value->app, index, &key) &&
            bongo_cat_preferences_import_sound_open(value->import_dialog, value->window, key.id);
    }
    case BONGO_UI_PREVIEW_SOUND: return bongo_cat_sounds_preview(value->app, index, &error);
    case BONGO_UI_CLEAR_SOUND: return bongo_cat_sounds_clear(value->app, index, &error);
    default: return false;
    }
}
static bool statistics_action(BongoCatPreferences *value, uint32_t op,
    uint32_t index, const void *data, uint32_t size) {
    BongoCatApp *app = value->app;
    BongoCatError error = {0};
    switch (op) {
    case BONGO_UI_BACKSPACE_ENABLE: return bongo_cat_statistics_backspace_enable(app, index != 0);
    case BONGO_UI_TIMER_ENABLE: return bongo_cat_statistics_timer_enable(app, index != 0);
    case BONGO_UI_RESET_STATS: return bongo_cat_statistics_reset(app, &error);
    case BONGO_UI_EXPORT_STATS: return bongo_cat_statistics_export(app, &error);
    case BONGO_UI_OPEN_FOLDER: return bongo_cat_platform_open_directory(app->data_root);
    case BONGO_UI_USAGE_ENABLE: return bongo_cat_statistics_usage_enable(app, index != 0);
    case BONGO_UI_EXPORT_USAGE: return bongo_cat_statistics_usage_export(app);
    case BONGO_UI_OVERLAY:
        if (!bongo_safe_statistics_overlay_set(app->settings.extensions_json,
                sizeof(app->settings.extensions_json), index != 0)) return false;
        app->statistics_overlay_retry_ns = 0;
        bongo_cat_statistics_overlay_update(app);
        return true;
    case BONGO_UI_SET_UNITS:
        if (!data || size != sizeof(BongoSafeStatisticsUnits)) return false;
        if (!bongo_safe_statistics_units_set(app->settings.extensions_json,
                sizeof(app->settings.extensions_json), data)) return false;
        value->font_reload_pending = true;
        return true;
    default: return false;
    }
}
static bool store_action(BongoCatPreferences *value, uint32_t op,
    uint32_t index, const void *data, uint32_t size) {
    BongoCatApp *app = value->app;
    BongoCatError error = {0};
    switch (op) {
    case BONGO_UI_SET_REPOSITORY:
        return !bongo_cat_store_busy(app) && data && size && size <= 512 &&
            memchr(data, 0, size) && bongo_safe_store_repository_set(app->settings.extensions_json,
                sizeof(app->settings.extensions_json), data);
    case BONGO_UI_REFRESH_STORE: return bongo_cat_store_refresh(app);
    case BONGO_UI_OPEN_URL:
        return data && size > 8 && size <= 2048 && memchr(data, 0, size) &&
            !memcmp(data, "https://", 8) && SDL_OpenURL(data);
    case BONGO_UI_DOWNLOAD: return bongo_cat_store_download(app, index);
    case BONGO_UI_UNINSTALL: {
        BongoCatStoreEntry entry;
        return bongo_cat_store_entry(app, index, &entry) && bongo_cat_store_uninstall(app, &entry, &error);
    }
    default: return false;
    }
}
uint32_t bongo_cat_preferences_plugin_action(void *context, uint32_t operation,
    uint32_t index, const void *data, uint32_t size) {
    BongoCatPluginPageBinding *binding = context;
    if (!binding || !binding->preferences) return 0;
    BongoCatPreferences *value = binding->preferences;
    if (operation == BONGO_UI_COPY_TEXT) {
        if (!data || size < 2 || size > 4096 || ((const char *)data)[size - 1] != '\0' ||
            memchr(data, '\0', size - 1)) return 0;
        return SDL_SetClipboardText(data);
    }
    bool ok = false;
    switch (binding->kind) {
    case BONGO_CAT_PLUGIN_UI_SOUNDS: ok = sounds_action(value, operation, index); break;
    case BONGO_CAT_PLUGIN_UI_ITEMS: ok = items_action(value, operation, index, data, size); break;
    case BONGO_CAT_PLUGIN_UI_STATISTICS: ok = statistics_action(value, operation, index, data, size); break;
    case BONGO_CAT_PLUGIN_UI_STORE: ok = store_action(value, operation, index, data, size); break;
    default: break;
    }
    value->render_dirty = true;
    return ok;
}
