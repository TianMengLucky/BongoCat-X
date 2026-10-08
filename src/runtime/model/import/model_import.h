#ifndef BONGO_CAT_MODEL_IMPORT_H
#define BONGO_CAT_MODEL_IMPORT_H

#include "bongo_cat/common.h"
#include "bongo_cat/model.h"

#define BONGO_CAT_IMPORT_CANDIDATE_CAP 16
#define BONGO_CAT_IMPORT_FAILURE_NAME_CAP 3
#define BONGO_CAT_SKIN_CONFIG_FILE "bongocat.skin.json"

typedef enum BongoCatImportFormat {
    BONGO_CAT_IMPORT_TAURI,
    BONGO_CAT_IMPORT_MVER,
    BONGO_CAT_IMPORT_MVER_PATCH,
    BONGO_CAT_IMPORT_INOCHI2D
} BongoCatImportFormat;

typedef struct BongoCatImportCandidate {
    char directory[BONGO_CAT_PATH_CAP];
    char setting[BONGO_CAT_PATH_CAP];
    char assets[BONGO_CAT_PATH_CAP];
    char package_root[BONGO_CAT_PATH_CAP];
    char patch_root[BONGO_CAT_PATH_CAP];
    char config[BONGO_CAT_PATH_CAP];
    char overrides[BONGO_CAT_PATH_CAP];
    BongoCatModelMode mode;
    BongoCatImportFormat format;
    bool gamepad_buttons;
} BongoCatImportCandidate;

typedef struct BongoCatImportDiscovery {
    BongoCatImportCandidate candidates[BONGO_CAT_IMPORT_CANDIDATE_CAP];
    /* Basename selected by the user. This becomes the visible model id. */
    char source_name[BONGO_CAT_ID_CAP];
    size_t count;
    int depth;
    bool ambiguous;
} BongoCatImportDiscovery;

typedef struct BongoCatImportReceipt {
    char ids[BONGO_CAT_IMPORT_CANDIDATE_CAP][BONGO_CAT_ID_CAP];
    bool installed[BONGO_CAT_IMPORT_CANDIDATE_CAP];
    size_t count;
    size_t installed_count;
} BongoCatImportReceipt;

typedef void (*BongoCatImportReceiptCallback)(void *userdata,
    const BongoCatImportReceipt *receipt);

typedef struct BongoCatImportBatchStats {
    size_t succeeded_count;
    size_t failed_count;
    size_t failure_name_count;
    char failure_names[BONGO_CAT_IMPORT_FAILURE_NAME_CAP][BONGO_CAT_ID_CAP];
} BongoCatImportBatchStats;

typedef struct BongoCatPackageMetadata {
    char package_id[BONGO_CAT_ID_CAP];
    char content_digest[65];
    char family_id[BONGO_CAT_ID_CAP];
    char display_name[BONGO_CAT_ID_CAP];
    char source_name[BONGO_CAT_ID_CAP];
    uint32_t capabilities;
} BongoCatPackageMetadata;

typedef struct BongoCatImportDigestCache BongoCatImportDigestCache;
typedef struct BongoCatImportSession BongoCatImportSession;

typedef BongoCatResult (*BongoCatImportVisitor)(void *userdata,
    const char *source, BongoCatImportDiscovery *discovery, BongoCatError *error);

typedef struct BongoCatApp BongoCatApp;

/* Shared discovery and package preparation. */
bool bongo_cat_import_discover(const char *source,
    BongoCatImportDiscovery *discovery, BongoCatError *error);
bool bongo_cat_import_adapter_metadata(const BongoCatImportCandidate *candidate,
    const char *target, BongoCatError *error);
bool bongo_cat_import_write_report(const BongoCatImportCandidate *candidate,
    const char *target, BongoCatError *error);
bool bongo_cat_import_prepare_adapter(const BongoCatImportCandidate *candidate,
    const char *target, BongoCatError *error);
bool bongo_cat_import_prepare_package(const BongoCatImportCandidate *candidate,
    const char *target, BongoCatImportCandidate *installed, BongoCatError *error);
bool bongo_cat_import_prepare_storage(
    const BongoCatImportDiscovery *discovery, const char *target,
    BongoCatError *error);
bool bongo_cat_import_authored_package(const char *directory);

/* Content identity and package metadata. */
bool bongo_cat_import_candidate_digest(const BongoCatImportCandidate *candidate,
    char output[65], BongoCatError *error);
bool bongo_cat_import_candidate_inspect(const BongoCatImportCandidate *candidate,
    char output[65], bool *placeholder, BongoCatError *error);
bool bongo_cat_import_candidate_inspect_cached(
    const BongoCatImportCandidate *candidate, char output[65],
    bool *placeholder, BongoCatImportDigestCache *cache,
    BongoCatError *error);
BongoCatImportDigestCache *bongo_cat_import_digest_cache_create(void);
void bongo_cat_import_digest_cache_destroy(BongoCatImportDigestCache *cache);
bool bongo_cat_import_digest_file_cached(BongoCatImportDigestCache *cache,
    const char *path, uint64_t size, uint64_t modified, char output[65]);
bool bongo_cat_import_prepare_package_metadata(
    BongoCatImportDiscovery *discovery,
    BongoCatPackageMetadata *metadata, BongoCatError *error);
bool bongo_cat_import_prepare_package_metadata_cached(
    BongoCatImportDiscovery *discovery, BongoCatPackageMetadata *metadata,
    BongoCatImportDigestCache *cache, BongoCatError *error);
bool bongo_cat_import_package_id(char *output, size_t capacity,
    const char *name);
bool bongo_cat_import_variant_id(char *output, size_t capacity,
    const char *package_id, size_t variant_index);
bool bongo_cat_import_package_base_id(char *output, size_t capacity,
    const char *model_id);
uint32_t bongo_cat_import_candidate_capabilities(
    const BongoCatImportCandidate *candidate);

/* Runtime adapter and catalog integration. */
void bongo_cat_import_describe_nearby_entry(BongoCatModelEntry *entry,
    const BongoCatImportCandidate *candidate, const char *id,
    const char *identity, const char *source_hash, const char *source,
    const char *adapter);
void bongo_cat_import_apply_metadata(BongoCatApp *app, const char *model_id,
    const char *directory);
bool bongo_cat_import_render_options(const char *directory,
    BongoCatModelRuntimeRenderOptions *options);

/* Recursive source scanning. */
BongoCatResult bongo_cat_import_scan(const char *root,
    BongoCatImportVisitor visitor, void *userdata, BongoCatError *error);
BongoCatResult bongo_cat_import_scan_diagnostic(const char *root,
    BongoCatImportVisitor visitor, void *userdata, BongoCatError *error);
BongoCatResult bongo_cat_import_scan_budget(const char *root,
    BongoCatImportVisitor visitor, void *userdata, uint64_t budget_ns,
    BongoCatError *error);
BongoCatResult bongo_cat_import_source_directory(const char *source,
    char *directory, size_t capacity, BongoCatError *error);

/* Nearby and installed catalog discovery. */
BongoCatResult bongo_cat_import_nearby_root(BongoCatApp *app,
    const char *root, BongoCatError *error);
BongoCatResult bongo_cat_import_nearby_scan(BongoCatApp *app,
    const char *root, BongoCatError *error);
BongoCatResult bongo_cat_import_nearby(BongoCatApp *app,
    const char *root, BongoCatError *error);
BongoCatResult bongo_cat_import_installed_models(BongoCatApp *app,
    const char *root, BongoCatError *error);
BongoCatResult bongo_cat_import_installed_package(BongoCatApp *app,
    const char *root, const char *package_id, BongoCatError *error);

/* Installation sessions and progressive batch import. */
BongoCatResult bongo_cat_import_install(const char *source,
    const char *models_root, BongoCatImportReceipt *receipt,
    BongoCatError *error);
BongoCatImportSession *bongo_cat_import_session_create(const char *models_root,
    BongoCatError *error);
void bongo_cat_import_session_destroy(BongoCatImportSession *session);
BongoCatResult bongo_cat_import_session_install(
    BongoCatImportSession *session, const char *source,
    BongoCatImportReceipt *receipt, BongoCatError *error);
BongoCatResult bongo_cat_import_session_install_progressive(
    BongoCatImportSession *session, const char *source,
    BongoCatImportReceiptCallback callback, void *userdata,
    BongoCatImportBatchStats *stats, BongoCatError *error);

#endif
