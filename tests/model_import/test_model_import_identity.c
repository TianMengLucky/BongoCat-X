#include "test_mver_support.h"

#include "model_import.h"
#include "model_import_mver.h"
#include "model_storage.h"
#include "runtime.h"
#include "bongo_cat/path.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(value) do { if (!(value)) { \
    fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #value); \
    failures++; \
} } while (0)

static void disabled_mver_live2d_is_not_imported(const char *temporary) {
    char root[BONGO_CAT_PATH_CAP], config[BONGO_CAT_PATH_CAP];
    snprintf(root, sizeof(root), "%s/bongocat-disabled-mver-%llu", temporary,
        (unsigned long long)SDL_GetTicksNS());
    CHECK(mver_fixture(root));
    CHECK(child(config, sizeof(config), root, "config.json", false));
    CHECK(write_text(config, "{\"standard\":{\"l2d\":false,"
        "\"keyboard\":[[65]],\"hand\":[[65]]}}"));
    BongoCatImportDiscovery discovery = {0};
    BongoCatError error = {0};
    CHECK(bongo_cat_import_mver_discover_exact(root, &discovery, &error) < 0);
    CHECK(discovery.count == 0);
    CHECK(strstr(error.message, "no supported model modes") != NULL);
    CHECK(bongo_cat_model_remove_tree(root, NULL));
}

static bool ends_with(const char *value, const char *suffix) {
    size_t value_length = strlen(value), suffix_length = strlen(suffix);
    return value_length >= suffix_length &&
        strcmp(value + value_length - suffix_length, suffix) == 0;
}

static void package_identity_rules(const BongoCatImportDiscovery *fixture) {
    static const char unicode_name[] =
        "\xE9\x9C\xB2\xE8\xA5\xBF\xE4\xBA\x9A-"
        "\xE8\xAA\x93\xE7\x84\xB0\xE7\x89\x88";
    BongoCatImportDiscovery discovery = *fixture;
    BongoCatPackageMetadata metadata[BONGO_CAT_IMPORT_CANDIDATE_CAP] = {0};
    BongoCatError error = {0};

    snprintf(discovery.source_name, sizeof(discovery.source_name), "%s",
        unicode_name);
    CHECK(bongo_cat_import_prepare_package_metadata(&discovery, metadata,
        &error));
    CHECK(discovery.count == 1 &&
        strcmp(metadata[0].package_id, unicode_name) == 0);

    discovery = *fixture;
    memset(metadata, 0, sizeof(metadata));
    snprintf(discovery.source_name, sizeof(discovery.source_name),
        "bad:name. ");
    CHECK(bongo_cat_import_prepare_package_metadata(&discovery, metadata,
        &error));
    CHECK(strcmp(metadata[0].package_id, "bad-name") == 0 &&
        strcmp(metadata[0].display_name, "bad:name. ") == 0);

    discovery = *fixture;
    memset(metadata, 0, sizeof(metadata));
    snprintf(discovery.source_name, sizeof(discovery.source_name), "CON.txt");
    CHECK(bongo_cat_import_prepare_package_metadata(&discovery, metadata,
        &error));
    CHECK(strcmp(metadata[0].package_id, "_CON.txt") == 0);

    discovery = *fixture;
    discovery.count = 2;
    discovery.candidates[1] = discovery.candidates[0];
    discovery.candidates[1].mode = BONGO_CAT_MODE_GAMEPAD;
    discovery.candidates[1].gamepad_buttons = true;
    memset(discovery.source_name, 'x', sizeof(discovery.source_name) - 1);
    discovery.source_name[sizeof(discovery.source_name) - 1] = '\0';
    memset(metadata, 0, sizeof(metadata));
    CHECK(bongo_cat_import_prepare_package_metadata(&discovery, metadata,
        &error));
    CHECK(discovery.count == 2 &&
        strlen(metadata[0].package_id) < BONGO_CAT_ID_CAP &&
        strlen(metadata[1].package_id) < BONGO_CAT_ID_CAP &&
        ends_with(metadata[1].package_id, "~2"));

    discovery = *fixture;
    discovery.count = 2;
    discovery.candidates[1] = discovery.candidates[0];
    snprintf(discovery.source_name, sizeof(discovery.source_name), "duplicate");
    memset(metadata, 0, sizeof(metadata));
    CHECK(bongo_cat_import_prepare_package_metadata(&discovery, metadata,
        &error));
    CHECK(discovery.count == 1 && strcmp(metadata[0].package_id,
        "duplicate") == 0);
}

int test_model_import_identity(void) {
    failures = 0;
    char *temporary = SDL_GetCurrentDirectory();
    CHECK(temporary != NULL);
    if (!temporary) return failures;
    disabled_mver_live2d_is_not_imported(temporary);
    char root[BONGO_CAT_PATH_CAP], data[BONGO_CAT_PATH_CAP];
    char source[BONGO_CAT_PATH_CAP], config[BONGO_CAT_PATH_CAP];
    char manifest[BONGO_CAT_PATH_CAP], missing[BONGO_CAT_PATH_CAP];
    unsigned long long nonce = (unsigned long long)SDL_GetTicksNS();
    snprintf(root, sizeof(root), "%s/bongocat-import-identity-%llu",
        temporary, nonce);
    snprintf(data, sizeof(data), "%s/bongocat-import-data-%llu",
        temporary, nonce);
    CHECK(SDL_CreateDirectory(root));
    CHECK(SDL_CreateDirectory(data));
    char models_root[BONGO_CAT_PATH_CAP];
    CHECK(child(models_root, sizeof(models_root), data, "models", true));
    CHECK(child(source, sizeof(source), root, "source", false));
    CHECK(mver_fixture(source));
    CHECK(child(config, sizeof(config), source, "config.json", false));
    CHECK(child(manifest, sizeof(manifest), source,
        "img/standard/cat_model/cat.model3.json", false));
    CHECK(child(missing, sizeof(missing), root, "missing", false));
    BongoCatError error = {0};
    BongoCatImportDiscovery discovered = {0};
    CHECK(bongo_cat_import_mver_discover(source, &discovered, &error) == 1);
    char digest[65];
    bool placeholder = true;
    CHECK(bongo_cat_import_candidate_inspect(&discovered.candidates[0],
        digest, &placeholder, &error) && !placeholder);
    package_identity_rules(&discovered);
    BongoCatImportReceipt first = {0}, repeated = {0}, wrapped = {0};
    BongoCatImportReceipt changed = {0};
    BongoCatImportSession *session = bongo_cat_import_session_create(models_root,
        &error);
    CHECK(session != NULL);
    CHECK(session && bongo_cat_import_session_install(session, config, &first,
        &error) == BONGO_CAT_OK);
    CHECK(first.count == 1 && first.installed_count == 1 && first.installed[0]);
    CHECK(strcmp(first.ids[0], "source") == 0);
    CHECK(strncmp(first.ids[0], "model-", 6) != 0);
    CHECK(session && bongo_cat_import_session_install(session, missing, NULL,
        &error) != BONGO_CAT_OK);
    error = (BongoCatError){0};
    CHECK(session && bongo_cat_import_session_install(session, manifest,
        &repeated, &error) == BONGO_CAT_OK);
    CHECK(repeated.count == 1 && repeated.installed_count == 0 &&
        !repeated.installed[0] && strcmp(first.ids[0], repeated.ids[0]) == 0);
    bongo_cat_import_session_destroy(session);
    CHECK(bongo_cat_import_install(manifest, models_root, &wrapped, &error) ==
        BONGO_CAT_OK);
    CHECK(wrapped.count == 1 && wrapped.installed_count == 0 &&
        strcmp(first.ids[0], wrapped.ids[0]) == 0);
    char extra[BONGO_CAT_PATH_CAP];
    CHECK(child(extra, sizeof(extra), source,
        "img/standard/cat_model/changed.txt", false));
    CHECK(write_text(extra, "changed"));
    CHECK(bongo_cat_import_install(source, models_root, &changed, &error) ==
        BONGO_CAT_OK);
    CHECK(changed.count == 1 && changed.installed_count == 1 &&
        changed.installed[0] && strcmp(changed.ids[0], "source-2") == 0);
    char store_package[BONGO_CAT_PATH_CAP], store_name[BONGO_CAT_ID_CAP];
    CHECK(child(store_package, sizeof(store_package), models_root, "source", false));
    CHECK(!bongo_cat_import_store_name_read(store_package, store_name, sizeof(store_name)));
    CHECK(bongo_cat_import_store_name_write(store_package, "Selena \"Store\""));
    CHECK(bongo_cat_import_store_name_read(store_package, store_name, sizeof(store_name)));
    CHECK(strcmp(store_name, "Selena \"Store\"") == 0);
    CHECK(!bongo_cat_import_store_name_write(store_package, ""));
    CHECK(!bongo_cat_import_store_name_read(store_package, store_name, 4));
    char adapter_metadata[BONGO_CAT_PATH_CAP];
    BongoCatApp *app = calloc(1, sizeof(*app));
    CHECK(app != NULL);
    if (app) {
        snprintf(app->models_root, sizeof(app->models_root), "%s", models_root);
        CHECK(child(app->cache_root, sizeof(app->cache_root), data, "cache",
            true));
        bongo_cat_models_init(&app->models);
        CHECK(bongo_cat_import_installed_models(app, models_root, &error) ==
            BONGO_CAT_OK);
        const BongoCatModelEntry *original = bongo_cat_models_find(&app->models,
            "source");
        const BongoCatModelEntry *modified = bongo_cat_models_find(&app->models,
            "source-2");
        CHECK(app->models.count == 2 && original && modified &&
            original->source_format == BONGO_CAT_MODEL_SOURCE_MVER &&
            original->content_digest[0] != '\0' &&
            strcmp(original->content_digest, modified->content_digest) != 0 &&
            (original->capabilities &
                BONGO_CAT_MODEL_CAPABILITY_MVER_PROJECTION));
        if (original) {
            CHECK(strcmp(original->display_name, "Selena \"Store\"") == 0);
            CHECK(strcmp(original->id, "source") == 0);
            CHECK(child(adapter_metadata, sizeof(adapter_metadata),
                original->adapter_directory,
                ".bongo-cat-adapter.json", false));
            CHECK(bongo_cat_path_is_file(adapter_metadata));
            char imported_config[BONGO_CAT_PATH_CAP];
            CHECK(child(imported_config, sizeof(imported_config),
                original->storage_directory, "config.json", false));
            CHECK(bongo_cat_path_is_file(imported_config));
            char internal[BONGO_CAT_PATH_CAP];
            CHECK(child(internal, sizeof(internal),
                original->storage_directory, ".bongo-cat-adapter", false));
            CHECK(!bongo_cat_path_is_dir(internal));
            CHECK(child(internal, sizeof(internal),
                original->storage_directory, ".bongo-cat-package.json",
                false));
            CHECK(!bongo_cat_path_is_file(internal));
        }
        free(app);
    }
    CHECK(bongo_cat_model_remove_tree(root, NULL));
    CHECK(bongo_cat_model_remove_tree(data, NULL));
    SDL_free(temporary);
    return failures;
}
