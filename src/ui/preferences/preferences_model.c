#include "preferences_state.h"
#include "bongo_cat/model_plugin.h"
#include "preferences_model_card.h"
#include "preferences_model_cover.h"
#include "preferences_notice.h"
#include "preferences_widgets.h"
#include "preferences_widgets_internal.h"
#include "ui_icons.h"
#include "model_import.h"
#include "runtime.h"
#include "bongo_cat/i18n.h"
#include "bongo_cat/overlay.h"
#include "bongo_cat/platform.h"
#include "bongo_cat/preferences.h"
#include "bongo_cat/log.h"

#include <stdio.h>
#include <string.h>

#define MODEL_CARD_HEIGHT 254
#define MODEL_LOAD_RENDER_INTERVAL_NS 100000000ull

static const char *tr(BongoCatApp *app, const char *key,
    const char *fallback) {
    return bongo_cat_i18n_get(app->i18n, key, fallback);
}

static float visual_wait_progress(uint64_t elapsed) {
    if (elapsed <= BONGO_CAT_MODEL_LOAD_VISUAL_RAMP_NS)
        return .8f * (float)((double)elapsed /
            BONGO_CAT_MODEL_LOAD_VISUAL_RAMP_NS);
    uint64_t slow_elapsed = elapsed - BONGO_CAT_MODEL_LOAD_VISUAL_RAMP_NS;
    uint64_t slow_duration = BONGO_CAT_MODEL_LOAD_VISUAL_DURATION_NS -
        BONGO_CAT_MODEL_LOAD_VISUAL_RAMP_NS;
    float slow = slow_duration ? (float)((double)slow_elapsed / slow_duration) : 1.0f;
    return .8f + (BONGO_CAT_MODEL_LOAD_VISUAL_WAIT_CAP - .8f) *
        NK_CLAMP(0.0f, slow, 1.0f);
}

void bongo_cat_preferences_model_visual_begin(BongoCatPreferences *value,
    const char *model_id) {
    if (!value || !model_id || !model_id[0]) return;
    value->model_load_visual_active = true;
    value->model_load_visual_started_ns = SDL_GetTicksNS();
    value->model_load_visual_completion_ns = 0;
    value->model_load_progress = 0.0f;
    snprintf(value->model_load_visual_id,
        sizeof(value->model_load_visual_id), "%s", model_id);
}

bool bongo_cat_preferences_model_texture_busy(const BongoCatPreferences *value) {
    return value && value->app && value->app->loading_model[0] &&
        value->app->model_load_runtime_stage == 2;
}

void bongo_cat_preferences_model_load_progress(BongoCatPreferences *value,
    float progress) {
    if (!value || !value->model_loading) return;
    uint64_t now = SDL_GetTicksNS();
    uint64_t elapsed = now - value->model_load_visual_started_ns;
    value->model_load_progress = visual_wait_progress(elapsed);
    value->render_dirty = true;
    /* Preserve the loading frame while transferring atlases. Switching to
       the shared settings context can serialize a software/virtual GPU and
       cost more than decoding. Native events still pump in the loader. */
    if (bongo_cat_preferences_model_texture_busy(value)) {
        ++value->model_load_render_deferred;
        return;
    }
    /* Slow software/virtual GPU presentation must not dominate model decode.
       Keep 10 Hz on fast drivers, backing off to at most 500 ms between
       loading frames; native events are pumped independently by the loader. */
    uint64_t interval = SDL_max(MODEL_LOAD_RENDER_INTERVAL_NS,
        SDL_min(value->model_load_render_cost_ns, 125000000ull) * 4);
    bool due = !value->model_load_render_ns ||
        now - value->model_load_render_ns >= interval;
    if (bongo_cat_preferences_visible(value) && due) {
        value->model_load_render_progress = progress;
        value->model_load_render_ns = now;
        bongo_cat_preferences_render(value);
        /* Expensive UI frames must not consume every decode callback. Count
           the interval from completion, including context swaps/presentation. */
        value->model_load_render_ns = SDL_GetTicksNS();
        value->model_load_render_cost_ns = value->model_load_render_ns - now;
        value->model_load_render_total_ns += value->model_load_render_cost_ns;
        ++value->model_load_render_count;
    }
}

float bongo_cat_preferences_model_visual_progress(BongoCatPreferences *value,
    const char *model_id) {
    if (!value || !model_id || !value->model_load_visual_active ||
        strcmp(value->model_load_visual_id, model_id)) return 0.0f;
    uint64_t now = SDL_GetTicksNS();
    uint64_t elapsed = now - value->model_load_visual_started_ns;
    if (!value->model_loading && !value->model_selection_pending) {
        if (!value->model_load_visual_completion_ns)
            value->model_load_visual_completion_ns = now;
        uint64_t completed = now - value->model_load_visual_completion_ns;
        float amount = NK_CLAMP(0.0f, (float)((double)completed /
            BONGO_CAT_MODEL_LOAD_VISUAL_COMPLETE_NS), 1.0f);
        float start = visual_wait_progress(elapsed);
        value->model_load_progress = start + (1.0f - start) * amount;
        if (amount >= 1.0f) {
            value->model_load_visual_active = false;
            value->model_load_visual_completion_ns = 0;
            value->model_load_progress = 1.0f;
        }
        value->render_dirty = true;
        return value->model_load_progress;
    }
    value->model_load_progress = visual_wait_progress(elapsed);
    value->render_dirty = true;
    return value->model_load_progress;
}

static void finish_model_load_progress(BongoCatPreferences *value) {
    if (!value) return;
    if (value->model_load_visual_active) {
        uint64_t now = SDL_GetTicksNS();
        value->model_load_visual_completion_ns = now;
        value->model_load_progress = visual_wait_progress(now -
            value->model_load_visual_started_ns);
    } else value->model_load_progress = 1.0f;
    value->render_dirty = true;
}

void bongo_cat_preferences_process_model_selection(BongoCatPreferences *value) {
    if (!value || !value->model_selection_pending || value->model_loading)
        return;
    char id[BONGO_CAT_ID_CAP];
    snprintf(id, sizeof(id), "%s", value->pending_model_id);
    bool active = value->pending_model_active;
    char previous[BONGO_CAT_ID_CAP];
    snprintf(previous, sizeof(previous), "%s", value->pending_model_replace_id);
    value->pending_model_replace_id[0] = '\0';
    value->pending_model_id[0] = '\0';
    value->model_selection_pending = false;
    value->pending_model_multiple = false;
    value->model_loading = true;
    value->model_load_progress = 0.0f;
    value->model_load_render_progress = 0.0f;
    value->model_load_render_ns = 0;
    value->model_load_render_cost_ns = 0;
    value->model_load_render_total_ns = 0;
    value->model_load_render_count = 0;
    value->model_load_render_deferred = 0;
    if (!value->model_load_visual_active || strcmp(value->model_load_visual_id, id))
        bongo_cat_preferences_model_visual_begin(value, id);
    snprintf(value->loading_model_id, sizeof(value->loading_model_id), "%s", id);
    BongoCatError error = {0};
    bongo_cat_preferences_resource_note(value, "before-model-switch");
    bool selected = previous[0] ? bongo_cat_app_replace_model_active(
        value->app, previous, id, &error) : id[0] ? bongo_cat_app_set_model_active(
        value->app, id, active, &error) : bongo_cat_app_clear_model(value->app, &error);
    if (selected && previous[0] && value->app->settings.model.multiple_pets) {
        const BongoCatModelEntry *mode = bongo_cat_models_find(&value->app->models, id);
        for (size_t i = 0; i < value->app->models.count; ++i) {
            const BongoCatModelEntry *other = &value->app->models.entries[i];
            if (strcmp(other->id, id) && bongo_cat_preferences_model_same_family(mode, other) &&
                bongo_cat_app_model_active(value->app, other->id))
                bongo_cat_app_set_model_active(value->app, other->id, false, &error);
        }
    }
    SDL_LogInfo(BONGO_CAT_LOG_LIFECYCLE,
        "[model-switch-ui] success=%d frames=%u total_ms=%.1f deferred=%u",
        selected, value->model_load_render_count,
        (double)value->model_load_render_total_ns / 1000000.0,
        value->model_load_render_deferred);
    if (selected && value->app->loaded_model[0]) finish_model_load_progress(value);
    else {
        value->model_load_progress = 0.0f;
        value->model_load_visual_active = false;
        value->model_load_visual_id[0] = '\0';
    }
    value->model_loading = false;
    value->loading_model_id[0] = '\0';
    if (!selected) SDL_LogError(SDL_LOG_CATEGORY_CUSTOM,
        "Model selection failed: id=%s error=%s", id,
        error.message[0] ? error.message : "Unable to display this model");
    bongo_cat_preferences_invalidate(value);
    /* Render on the next input frame. A nested render here would replay the
       card click that queued this selection and immediately toggle it back. */
}

static void smoke_model_behavior(BongoCatPreferences *value) {
    BongoCatApp *app = value->app;
    if (app->smoke_preference_model_select) {
        for (size_t i = 0; i < app->models.count; ++i) {
            const BongoCatModelEntry *entry = &app->models.entries[i];
            if (entry->preset || !strcmp(entry->id,
                app->session.active_model_id)) continue;
            app->smoke_preference_model_select = false;
            value->smoke_behavior_open_pending = true;
            SDL_Log("Preferences smoke selecting model %s", entry->id);
            bongo_cat_preferences_model_select(value, entry);
            return;
        }
    }
    if (value->smoke_behavior_open_pending && !value->font_reload_pending &&
        !value->model_selection_pending && !value->model_loading) {
        value->smoke_behavior_open_pending = false;
        bongo_cat_preferences_behavior_dialog_open(value);
    }
}

static bool model_hidden(const BongoCatPreferences *value, const char *id) {
    return bongo_cat_settings_model_hidden(&value->app->settings, id);
}

static size_t visible_model_count(const BongoCatPreferences *value,
    bool managed, bool hidden) {
    size_t count = 0;
    for (size_t i = 0; i < value->app->models.count; ++i) {
        const BongoCatModelEntry *entry = &value->app->models.entries[i];
        if (entry->managed == managed &&
            model_hidden(value, entry->id) == hidden) count++;
    }
    return count;
}

static int preset_model_order(const BongoCatModelEntry *entry) {
    if (!entry || !entry->preset) return 3;
    if (!strcmp(entry->id, "standard")) return 0;
    if (!strcmp(entry->id, "keyboard")) return 1;
    if (!strcmp(entry->id, "gamepad")) return 2;
    return 3;
}

static void draw_models(BongoCatPreferences *value,
    struct nk_context *context, bool managed, bool storage_busy, bool hidden) {
    for (int order = 0; order <= 3; ++order) {
        for (size_t i = 0; i < value->app->models.count; ++i) {
            const BongoCatModelEntry *entry = &value->app->models.entries[i];
            if (entry->managed != managed || preset_model_order(entry) != order)
                continue;
            if (model_hidden(value, entry->id) != hidden) continue;
            const BongoCatModelEntry *shown =
                bongo_cat_preferences_model_group_entry(value, entry, hidden);
            if (shown) bongo_cat_preferences_model_card(value, context, shown,
                storage_busy);
        }
    }
}

static bool builtin_models_missing(const BongoCatApp *app) {
    static const char *const names[] = {"standard", "keyboard", "gamepad"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
        if (!bongo_cat_models_find(&app->models, names[i])) return true;
    return false;
}

void bongo_cat_preferences_page_model(BongoCatPreferences *value,
    struct nk_context *context) {
    BongoCatApp *app = value->app;
    bongo_cat_pref_intro(context, tr(app, "pages.preference.navigation.localModelsHint",
        "Select a model card to display it. Import local models here, or switch to Store to browse community models."));
    smoke_model_behavior(value);
    bool show_hidden = value->model_show_hidden &&
        (visible_model_count(value, false, true) ||
        visible_model_count(value, true, true));
    value->model_show_hidden = show_hidden;
    bongo_cat_preferences_model_covers_begin(app);
    bool multiple = app->settings.model.multiple_pets;
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_MULTIPLE_MODELS);
    if (bongo_cat_pref_toggle(context, "multiple-pets", tr(app,
            "native.multiplePets",
            "Display multiple"), tr(app, "native.multiplePetsHint",
            "Select several models to display up to eight pets. Disable to return to a single selection."), &multiple))
        bongo_cat_app_set_multiple_pets(app, multiple);
    /* Runtime-Core builds: offer the Live2D Core import while rendering is
       still disabled; the row disappears as soon as the Core is loaded. The
       sync icon re-runs the drop-in folder scan on demand. */
    if (bongo_cat_platform_live2d_core_import_supported() &&
        !bongo_cat_platform_live2d_core_available()) {
        bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_LIVE2D_CORE);
        int action = bongo_cat_pref_button_with_icon(context,
            "live2d-core-import",
            tr(app, "native.live2dCoreImport", "Import Live2D Core"),
            tr(app, "native.live2dCoreImportHint",
            "Select the Cubism Core library or the official Cubism SDK zip to enable Live2D rendering without a restart"),
            BONGO_CAT_UI_ICON_SYNC,
            tr(app, "native.live2dCoreImportButton", "Choose file"));
        if (action == 1) {
            SDL_LogInfo(SDL_LOG_CATEGORY_CUSTOM,
                "Live2D Core import requested from settings");
            value->sdk_import_requested = true;
        } else if (action == 2) {
            /* The scan (zip extraction included) runs on a worker thread;
               the result toast comes from the completion event handler. */
            bongo_cat_preferences_request_live2d_rescan(value);
            bongo_cat_preferences_notice_show(app, bongo_cat_i18n_get(
                app->i18n, "native.live2dCoreScanning",
                "Scanning the live2d folders for the Cubism Core..."), false);
        }
    }
    /* Deleted built-in models can be copied back from the app assets; the
       row only appears while one of them is missing from the catalog. */
    if (builtin_models_missing(app)) {
        bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_SECTION_MODEL);
        int action = bongo_cat_pref_button_with_icon(context,
            "restore-builtins",
            tr(app, "native.restoreBuiltins", "Restore default models"),
            tr(app, "native.restoreBuiltinsHint",
                "One or more built-in models were deleted; restore them from the application assets"),
            BONGO_CAT_UI_ICON_SYNC,
            tr(app, "native.restoreBuiltinsButton", "Restore"));
        if (action == 1 || action == 2) {
            BongoCatError error = {0};
            if (bongo_cat_app_restore_builtins(app, &error) == BONGO_CAT_OK)
                bongo_cat_preferences_notice_show(app, tr(app,
                    "native.restoreBuiltinsDone",
                    "Default models restored"), false);
            else
                bongo_cat_preferences_notice_show(app, tr(app,
                    "native.restoreBuiltinsFailed",
                    "Failed to restore the default models"), true);
        }
    }
    if (bongo_cat_preferences_model_section(value, context) &&
        !SDL_OpenURL("https://bongocat.pet/models"))
        SDL_LogWarn(SDL_LOG_CATEGORY_CUSTOM,
            "Cannot open model library: %s", SDL_GetError());
    float width = nk_window_get_content_region(context).w;
    /* Keep room for translated names and mode controls at every window size. */
    const float minimum_card_width = 232.0f;
    const float card_gap = 14.0f;
    int columns = NK_CLAMP(1, (int)((width + card_gap) /
        (minimum_card_width + card_gap)), 4);
    struct nk_vec2 old_spacing = context->style.window.spacing;
    context->style.window.spacing = nk_vec2(14, 17);
    const float import_text_width = NK_MAX(1.0f, width - 96.0f);
    int import_lines = bongo_cat_pref_detail_text(context, tr(app,
        "pages.preference.model.hints.clickOrDragToImport",
        "Click or drag a model folder or ZIP"),
        nk_rect(0, 0, import_text_width, 0), false);
    import_lines = NK_MAX(import_lines, bongo_cat_pref_detail_text(context, tr(app,
        "pages.preference.model.hints.importingBackground", "Importing in background..."),
        nk_rect(0, 0, import_text_width, 0), false));
    nk_layout_row_dynamic(context, NK_MAX(88.0f, 32.0f + import_lines * 19.0f), 1);
    if (bongo_cat_preferences_model_import_card(value, context))
        bongo_cat_preferences_request_model_import(app->preferences);
    nk_layout_row_dynamic(context, MODEL_CARD_HEIGHT, columns);
    bool storage_busy = bongo_cat_preferences_import_status(
        value->import_dialog, NULL, NULL, NULL) ||
        bongo_cat_app_model_refresh_busy(app);
    draw_models(value, context, false, storage_busy, show_hidden);
    if (visible_model_count(value, true, show_hidden)) {
        bongo_cat_pref_section(context,
            tr(app, "pages.preference.model.nearbyTitle", "Nearby models"));
        nk_layout_row_dynamic(context, MODEL_CARD_HEIGHT, columns);
        draw_models(value, context, true, storage_busy, show_hidden);
    }
    context->style.window.spacing = old_spacing;
    bongo_cat_preferences_model_covers_prune(app);
}

/* Pointer-input rows of the cat settings page: sensitivity, ignore-mouse and
   the Windows force-raw-mouse-input toggle. Drawn from here so the page
   module stays within the source size policy. */
void bongo_cat_preferences_mouse_input_rows(BongoCatApp *app,
    BongoCatModelPreferences *model, struct nk_context *context) {
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_MOUSE_SENSITIVITY);
    float sensitivity = bongo_cat_settings_mouse_sensitivity(&app->settings);
    if (bongo_cat_pref_slider_suffix(context, "mouse-sensitivity", tr(app,
        "pages.preference.cat.labels.mouseSensitivity", "Desktop Pet Mouse Sensitivity"), tr(app,
        "pages.preference.cat.hints.mouseSensitivity",
        "Multiply mouse movement by 0 to 5 in steps of 0.01. Default: 1.00; 0 stops the pet mouse. Double-click the value to reset."),
        0.0f, &sensitivity, BONGO_CAT_MAX_MOUSE_SENSITIVITY, 0.01f,
        BONGO_CAT_DEFAULT_MOUSE_SENSITIVITY, NULL)) {
        bongo_cat_settings_set_mouse_sensitivity(&app->settings, sensitivity);
        /* Keep the virtual game pointer in place while tuning its speed. */
        app->pointer_known = false;
        app->dirty = true;
    }
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_IGNORE_MOUSE);
    if (bongo_cat_pref_toggle(context, "ignore-mouse", tr(app,
        "pages.preference.cat.labels.ignoreMouse", "Ignore Mouse Events"), tr(app, "pages.preference.cat.hints.ignoreMouse",
            "Stop mouse movement and buttons from driving the model; keyboard and gamepad input remain active."),
        &model->ignore_mouse)) {
        app->pointer_known = false;
        app->dirty = true;
    }
#ifdef _WIN32
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_FORCE_MOUSE);
    if (bongo_cat_pref_toggle(context, "force-mouse-input", tr(app,
        "pages.preference.cat.labels.forceMouseInput", "Force Mouse Input"),
        tr(app, "pages.preference.cat.hints.forceMouseInput",
            "Use raw mouse motion on Windows to follow a hidden or locked game cursor."),
        &model->force_mouse_input))
        bongo_cat_app_reset_pointer_tracking(app);
#endif
}

/* Model-section rows of the cat settings page: gamepad four-hand mode, the
   multi-key display toggle and its layout editor entry. Kept here like the
   pointer rows so the page module stays within the source size policy. */
void bongo_cat_preferences_model_hands_rows(BongoCatPreferences *value,
    struct nk_context *context) {
    BongoCatApp *app = value->app;
    BongoCatModelPreferences *model = &app->settings.model;
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_GAMEPAD_FOUR_HANDS);
    if (bongo_cat_pref_toggle(context, "gamepad-four-hands", tr(app,
        "pages.preference.cat.labels.gamepadFourHands", "Gamepad Four-Hand Mode"), tr(app,
        "pages.preference.cat.hints.gamepadFourHands", "Keep two extra hands visible in gamepad mode, if the model supplies them."),
        &model->gamepad_four_hands))
        bongo_cat_app_refresh_hands(app);
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_GAMEPAD_FOUR_HANDS);
    if (bongo_cat_pref_toggle(context, "multi-key-display", tr(app,
        "pages.preference.cat.labels.multiKeyDisplay", "Multi-Key Display"), tr(app,
        "pages.preference.cat.hints.multiKeyDisplay",
        "Mver only: join held keys with + in one bubble per side. Mirrored characters stay readable; edit the crop regions below."),
        &model->multi_key_display)) {
        bongo_cat_overlay_set_multi_key_display(app->overlay,
            model->multi_key_display);
        bongo_cat_app_reapply_input(app);
        app->dirty = true;
    }
    /* The editor only appears once the model supplies a key folder to crop. */
    if (model->multi_key_display && app->overlay &&
        (bongo_cat_overlay_multi_key_layout_editable(app->overlay, false) ||
         bongo_cat_overlay_multi_key_layout_editable(app->overlay, true))) {
        bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_GAMEPAD_FOUR_HANDS);
        if (bongo_cat_pref_button(context, "multi-key-layout", tr(app,
            "pages.preference.cat.labels.multiKeyLayout", "Multi-Key Layout"), tr(app,
            "pages.preference.cat.hints.multiKeyLayout",
            "Crop the hand, bubble and character regions of the key sprite. Saved to the model's hand-layout.json."),
            tr(app, "pages.preference.cat.labels.multiKeyLayoutOpen",
            "Configure")))
            bongo_cat_preferences_layout_dialog_open(value);
    }
}
