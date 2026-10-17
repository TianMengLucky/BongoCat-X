#include "mver_render.h"
#include "bongo_cat/json.h"
#include "bongo_cat/safe_import.h"

void bongo_cat_mver_render_options(BongoJsonValue *config,
    BongoCatModelRuntimeRenderOptions *options) {
    if (!bongo_safe_mver_render_options(config, options, sizeof(*options)) && options)
        *options = (BongoCatModelRuntimeRenderOptions){0};
}
bool bongo_cat_mver_render_read(const char *path,
    BongoCatModelRuntimeRenderOptions *options) {
    BongoJsonDoc *doc = bongo_cat_json_read_file(path,
        BONGO_JSON_READ_JSON5 | BONGO_JSON_READ_ALLOW_INVALID_UNICODE, NULL);
    bool ok = doc && bongo_json_is_obj(bongo_json_doc_get_root(doc));
    if (ok) bongo_cat_mver_render_options(bongo_json_doc_get_root(doc), options);
    bongo_json_doc_free(doc);
    return ok;
}
