#include "test.h"
#include "bongo_cat/model_plugins.h"
#include "bongo_cat/model_plugin_host.h"
#include "bongo_cat/path.h"
#include "bongo_cat/platform.h"
#include "bongo_cat/file.h"
#include <SDL3/SDL.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

int bongo_cat_test_failures;

int main(int argc, char **argv) {
    if (argc != 3 && argc != 7 && argc != 8) return 2;
    BongoCatError error = {0};
    BongoCatModelPluginInfo info;
    BongoCatModelEngine engine = 0;
    const BongoCatModelEngine inox = BONGO_CAT_MODEL_ENGINE_INOX2D;
#ifdef _WIN32
    HKEY isolated_registry = NULL;
    wchar_t registry_name[128];
    if (argc == 8) {
        swprintf(registry_name, 128, L"Software\\BongoCat\\Tests\\PluginArchive-%lu",
            GetCurrentProcessId());
        CHECK(RegCreateKeyExW(HKEY_CURRENT_USER, registry_name, 0, NULL,
            REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &isolated_registry, NULL) == ERROR_SUCCESS);
        CHECK(RegOverridePredefKey(HKEY_CURRENT_USER, isolated_registry) == ERROR_SUCCESS);
    }
#endif
    CHECK(SDL_Init(0));
    bongo_cat_model_plugins_set_root(argv[2]);
    if (argc >= 7) {
        CHECK(bongo_cat_model_plugin_install(argv[3], &engine, &error));
        CHECK(engine == inox);
        if (argc == 8) {
            CHECK(!bongo_cat_platform_live2d_core_available());
            bool imported = bongo_cat_model_plugin_install(argv[7], &engine, &error);
            if (!imported) fprintf(stderr, "Core ZIP import: %s\n", error.message);
            CHECK(imported);
            CHECK(bongo_cat_platform_live2d_core_available());
        }
        CHECK(bongo_cat_model_plugin_install(argv[4], &engine, &error));
        for (int i = 5; i < 7; ++i) {
            CHECK(!bongo_cat_model_plugin_install(argv[i], &engine, &error));
            CHECK(error.code == BONGO_CAT_ERROR_FORMAT);
        }
        bongo_cat_model_plugin_info(inox, &info);
        CHECK(info.installed && info.managed && info.enabled);
    }
    CHECK(bongo_cat_model_plugin_install(argv[1], &engine, &error));
    CHECK(engine == inox);
    bongo_cat_model_plugin_info(inox, &info);
    CHECK(info.installed && info.enabled && info.managed && !info.active);
    SDL_SharedObject *library = SDL_LoadObject(info.path);
    CHECK(library != NULL);
    if (library) {
        union { SDL_FunctionPointer function; BongoCatModelPluginQuery query; } entry;
        entry.function = SDL_LoadFunction(library, BONGO_CAT_MODEL_PLUGIN_SYMBOL);
        CHECK(entry.function != NULL);
        if (entry.function) {
            const BongoCatModelPluginHost *host = bongo_cat_model_plugin_host();
            CHECK(entry.query(BONGO_CAT_MODEL_PLUGIN_ABI + 1, host) == NULL);
            const BongoCatModelPlugin *plugin = entry.query(BONGO_CAT_MODEL_PLUGIN_ABI, host);
            CHECK(bongo_cat_model_plugin_descriptor_valid(plugin, inox));
            CHECK(!bongo_cat_model_plugin_descriptor_valid(plugin, BONGO_CAT_MODEL_ENGINE_LIVE2D));
            if (plugin) {
                BongoCatModelRuntime *runtime = plugin->ops.create("", &error);
                CHECK(runtime != NULL);
                CHECK(!plugin->ops.ready(runtime));
                if (runtime) plugin->ops.destroy(runtime);
            }
        }
        SDL_UnloadObject(library);
    }
    CHECK(bongo_cat_model_plugin_set_enabled(inox, false, &error));
    bongo_cat_model_plugin_info(inox, &info);
    CHECK(info.installed && !info.enabled);
    CHECK(bongo_cat_model_plugin_set_enabled(inox, true, &error));
    bongo_cat_model_plugin_retain(inox);
    CHECK(!bongo_cat_model_plugin_remove(inox, &error));
    CHECK(!bongo_cat_model_plugin_install(argv[1], &engine, &error));
    bongo_cat_model_plugin_release(inox);
    CHECK(bongo_cat_model_plugin_remove(inox, &error));
    bongo_cat_model_plugin_info(inox, &info);
    CHECK(!info.managed && !info.enabled && !info.active);
    CHECK(bongo_cat_model_plugin_set_enabled(inox, true, &error));
    SDL_Quit();
#ifdef _WIN32
    if (isolated_registry) {
        CHECK(RegOverridePredefKey(HKEY_CURRENT_USER, NULL) == ERROR_SUCCESS);
        RegCloseKey(isolated_registry);
        CHECK(RegDeleteKeyW(HKEY_CURRENT_USER, registry_name) == ERROR_SUCCESS);
    }
#endif
    return bongo_cat_test_failures ? 1 : 0;
}
