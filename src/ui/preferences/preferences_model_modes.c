#include "preferences_model_card.h"
#include "preferences_state.h"
#include "preferences_controls.h"
#include "preferences_notice.h"
#include "runtime.h"
#include "bongo_cat/i18n.h"

#include <stdio.h>
#include <string.h>

static bool builtin_mode(const BongoCatModelEntry *entry) {
    return entry->preset && (!strcmp(entry->id, "standard") ||
        !strcmp(entry->id, "keyboard") || !strcmp(entry->id, "gamepad"));
}

bool bongo_cat_preferences_model_same_family(const BongoCatModelEntry *a,
    const BongoCatModelEntry *b) {
    if (!a || !b || a->managed != b->managed) return false;
    if (!strcmp(a->id, b->id)) return true;
    if (builtin_mode(a) && builtin_mode(b)) return true;
    return a->family_id[0] && b->family_id[0] &&
        !strcmp(a->family_id, b->family_id);
}

static bool visible_mode(const BongoCatPreferences *value,
    const BongoCatModelEntry *entry, bool hidden) {
    return bongo_cat_settings_model_hidden(&value->app->settings, entry->id) == hidden;
}

const BongoCatModelEntry *bongo_cat_preferences_model_group_entry(
    const BongoCatPreferences *value, const BongoCatModelEntry *seed, bool hidden) {
    const BongoCatModelEntry *shown = seed;
    bool reached = false;
    for (size_t i = 0; i < value->app->models.count; ++i) {
        const BongoCatModelEntry *entry = &value->app->models.entries[i];
        if (entry == seed) reached = true;
        if (!visible_mode(value, entry, hidden) ||
            !bongo_cat_preferences_model_same_family(seed, entry)) continue;
        if (!reached) return NULL; /* Only the first member draws a card. */
        if (!strcmp(entry->id, value->app->session.active_model_id)) shown = entry;
        else if (shown == seed && bongo_cat_app_model_active(value->app, entry->id))
            shown = entry;
    }
    if (value->model_selection_pending) {
        const BongoCatModelEntry *pending = bongo_cat_models_find(&value->app->models,
            value->pending_model_id);
        if (pending && visible_mode(value, pending, hidden) &&
            bongo_cat_preferences_model_same_family(seed, pending)) shown = pending;
    }
    return shown;
}

static const char *mode_label(BongoCatApp *app, BongoCatModelMode mode) {
    const char *key = mode == BONGO_CAT_MODE_KEYBOARD ?
        "pages.preference.model.labels.modeKeyboard" : mode == BONGO_CAT_MODE_GAMEPAD ?
        "pages.preference.model.labels.modeGamepad" : "pages.preference.model.labels.modeStandard";
    return bongo_cat_i18n_get(app->i18n, key, bongo_cat_mode_name(mode));
}

bool bongo_cat_preferences_model_mode_selector(BongoCatPreferences *value,
    struct nk_context *context, const BongoCatModelEntry *entry, struct nk_rect bounds) {
    const BongoCatModelEntry *modes[BONGO_CAT_MODEL_CAP];
    const char *labels[BONGO_CAT_MODEL_CAP];
    int count = 0, selected = 0;
    bool hidden = bongo_cat_settings_model_hidden(&value->app->settings, entry->id);
    for (size_t i = 0; i < value->app->models.count; ++i) {
        const BongoCatModelEntry *mode = &value->app->models.entries[i];
        if (!visible_mode(value, mode, hidden) ||
            !bongo_cat_preferences_model_same_family(entry, mode)) continue;
        if (!strcmp(mode->id, entry->id)) selected = count;
        modes[count] = mode;
        labels[count++] = mode_label(value->app, mode->mode);
    }
    if (count < 2) return false;
    // Distinguish authored variants that share an input mode.
    for (int i = 0; i < count; ++i)
        for (int j = 0; j < count; ++j)
            if (i != j && modes[i]->mode == modes[j]->mode) {
                labels[i] = bongo_cat_model_name(&value->app->settings, modes[i]);
                break;
            }
    // Put the existing combo into the card without changing the surrounding grid.
    struct nk_panel *panel = context->current->layout;
    struct nk_panel saved = *panel;
    nk_layout_space_begin(context, NK_STATIC, bounds.h, 1);
    struct nk_vec2 local = nk_layout_space_to_local(context, nk_vec2(bounds.x, bounds.y));
    nk_layout_space_push(context, nk_rect(local.x, local.y, bounds.w, bounds.h));
    int choice = bongo_cat_pref_control_combo(context, entry->family_id[0] ?
        entry->family_id : "builtin-modes", labels, count, selected);
    nk_layout_space_end(context);
    panel->at_x = saved.at_x;
    panel->at_y = saved.at_y;
    panel->max_x = saved.max_x;
    panel->row = saved.row;
    bool hovered = nk_input_is_mouse_hovering_rect(&context->input, bounds);
    if (choice != selected && !value->model_loading) {
        const BongoCatModelEntry *next = modes[choice];
        // A replacement bypasses the eight-pet limit and never toggles off the new mode.
        const BongoCatModelEntry *previous = NULL;
        for (int i = 0; i < count; ++i)
            if (bongo_cat_app_model_active(value->app, modes[i]->id)) {
                previous = modes[i];
                if (!strcmp(previous->id, value->app->session.active_model_id)) break;
            }
        if (!previous && value->app->settings.model.multiple_pets &&
            bongo_cat_app_active_model_count(value->app) >= BONGO_CAT_ACTIVE_MODEL_CAP) {
            bongo_cat_preferences_notice_show(value->app, bongo_cat_i18n_get(value->app->i18n,
                "native.multiplePetsLimit", "Up to 8 desktop pets can be displayed at once"), true);
            return true;
        }
        context->input.mouse.buttons[NK_BUTTON_LEFT].clicked = 0;
        snprintf(value->pending_model_id, sizeof(value->pending_model_id), "%s", next->id);
        snprintf(value->pending_model_replace_id, sizeof(value->pending_model_replace_id),
            "%s", previous ? previous->id : "");
        value->pending_model_active = true;
        value->pending_model_multiple = value->app->settings.model.multiple_pets;
        value->model_selection_pending = true;
        bongo_cat_preferences_model_visual_begin(value, next->id);
        value->render_dirty = true;
    }
    return hovered || context->current->popup.active || choice != selected;
}
