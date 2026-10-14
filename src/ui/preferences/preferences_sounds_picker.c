#include "preferences_state.h"
#include "preferences_import_internal.h"
#include "bongo_cat/preferences.h"
bool bongo_cat_preferences_sounds_blocked(const BongoCatPreferences *value) {
    return value && (bongo_cat_preferences_shortcuts_blocked(value) ||
        bongo_cat_preferences_sound_picker_open(value) ||
        bongo_cat_preferences_remove_dialog_active(value->app) ||
        bongo_cat_preferences_behavior_dialog_active(value) ||
        bongo_cat_preferences_random_dialog_active(value));
}
bool bongo_cat_preferences_sound_picker_open(const BongoCatPreferences *value) {
    if (!value || !value->import_dialog) return false;
    BongoCatImportDialog *dialog = value->import_dialog;
    SDL_LockMutex(dialog->mutex);
    bool open = dialog->open && dialog->sounds_key[0];
    SDL_UnlockMutex(dialog->mutex);
    return open;
}
