#include "model_import.h"
#include "bongo_cat/safe_import.h"
bool bongo_cat_import_adapter_metadata(const BongoCatImportCandidate *candidate,
    const char *target, BongoCatError *error) {
    if (!candidate || !target) return false;
    BongoSafeImportMetadata input = {candidate->config, candidate->directory,
        candidate->setting, candidate->assets, candidate->overrides, target,
        bongo_cat_mode_name(candidate->mode), (int)candidate->format, candidate->gamepad_buttons};
    char message[256] = {0};
    bool ok = bongo_safe_import_metadata(&input, sizeof(input), message, sizeof(message));
    if (!ok) bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT, "Cannot create runtime adapter metadata: %s", message);
    return ok;
}
