#include "preferences_state.h"
#include "preferences_model_card.h"
#include "preferences_model_cover.h"
#include "preferences_notice.h"
#include "preferences_widgets.h"
#include "ui_icons.h"
#include "model_import.h"
#include "runtime.h"
#include "bongo_cat/i18n.h"
#include "bongo_cat/platform.h"
#include "bongo_cat/preferences.h"
#include "bongo_cat/log.h"

#include <stdio.h>
#include <string.h>

#define MODEL_CARD_HEIGHT 214
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
    bool selected = id[0] ? bongo_cat_app_set_model_active(
        value->app, id, active, &error) : bongo_cat_app_clear_model(value->app, &error);
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
            bongo_cat_preferences_model_card(value, context, entry,
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
            "Display multiple"), "", &multiple))
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
            "Select the Cubism Core library or the official Cubism SDK "
            "zip to enable Live2D rendering without a restart"),
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
                "One or more built-in models were deleted; restore them "
                "from the application assets"),
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
    int columns = width >= 780 ? 4 : width >= 620 ? 3 : width >= 400 ? 2 : 1;
    struct nk_vec2 old_spacing = context->style.window.spacing;
    context->style.window.spacing = nk_vec2(14, 17);
    nk_layout_row_dynamic(context, MODEL_CARD_HEIGHT, columns);
    if (bongo_cat_preferences_model_import_card(value, context))
        bongo_cat_preferences_request_model_import(app->preferences);
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

/* Pointer-input rows of the cat settings page: the ignore-mouse toggle and
   the Windows force-raw-mouse-input toggle. Drawn from here so the page
   module stays within the source size policy. */
void bongo_cat_preferences_mouse_input_rows(BongoCatApp *app,
    BongoCatModelPreferences *model, struct nk_context *context) {
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_IGNORE_MOUSE);
    if (bongo_cat_pref_toggle(context, "ignore-mouse", tr(app,
        "pages.preference.cat.labels.ignoreMouse", "Ignore Mouse Events"), "",
        &model->ignore_mouse)) {
        app->pointer_known = false;
        app->dirty = true;
    }
#ifdef _WIN32
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_FORCE_MOUSE);
    if (bongo_cat_pref_toggle(context, "force-mouse-input", tr(app,
        "pages.preference.cat.labels.forceMouseInput", "Force Mouse Input"),
        tr(app, "pages.preference.cat.hints.forceMouseInput",
            "Drive the model with raw device motion so it keeps following the mouse when a game hides the cursor"),
        &model->force_mouse_input))
        bongo_cat_app_reset_pointer_tracking(app);
#endif
}

/* Render backend row of the general page (kept here like the pointer rows
   so the page module stays within the source size policy). Switching asks
   the app loop to hot-rebuild the render stack at the next frame
   boundary. */
void bongo_cat_preferences_render_backend_row(BongoCatApp *app,
    BongoCatApplicationPreferences *options, struct nk_context *context) {
    const char *items[4];
    BongoCatRenderBackend values[4];
    int count = 0, selected = 0;
    items[count] = tr(app, "pages.preference.general.options.auto", "System");
    values[count] = BONGO_CAT_RENDER_BACKEND_AUTO;
    if (options->render_backend == BONGO_CAT_RENDER_BACKEND_AUTO)
        selected = count;
    count++;
    items[count] = "OpenGL";
    values[count] = BONGO_CAT_RENDER_BACKEND_OPENGL;
    if (options->render_backend == BONGO_CAT_RENDER_BACKEND_OPENGL)
        selected = count;
    count++;
#if defined(_WIN32) || (defined(__linux__) && defined(__x86_64__))
    BongoCatRhi vulkan = {.backend = BONGO_CAT_RHI_VULKAN};
    if (bongo_cat_rhi_backend_available(vulkan.backend) && bongo_cat_rhi_live2d_supported(&vulkan)) {
        items[count] = "Vulkan";
        values[count] = BONGO_CAT_RENDER_BACKEND_VULKAN;
        if (options->render_backend == BONGO_CAT_RENDER_BACKEND_VULKAN)
            selected = count;
        count++;
    }
#endif
#ifdef __APPLE__
    BongoCatRhi metal = {.backend = BONGO_CAT_RHI_METAL};
    if (bongo_cat_rhi_live2d_supported(&metal)) {
        items[count] = "Metal";
        values[count] = BONGO_CAT_RENDER_BACKEND_METAL;
        if (options->render_backend == BONGO_CAT_RENDER_BACKEND_METAL)
            selected = count;
        count++;
    }
#endif
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_RENDER_QUALITY);
    int next = bongo_cat_pref_combo(context, "render-backend", tr(app,
        "pages.preference.general.labels.renderBackend", "Render Backend"),
        tr(app, "pages.preference.general.hints.renderBackend",
            "Switch immediately without restarting. Vulkan/Metal support Live2D experimentally. All backends support desk backgrounds and rounded corners; key/effect/pointer overlays require OpenGL"),
        items, count, selected);
    if (values[next] != options->render_backend) {
        options->render_backend = values[next];
        app->render_backend_swap_pending = true;
        bongo_cat_preferences_notice_show(app, tr(app,
            "pages.preference.general.hints.renderBackendChanged",
            "Switching the render backend"), false);
    }
}

void bongo_cat_preferences_large_render_row(BongoCatApp *app,
    BongoCatApplicationPreferences *options, struct nk_context *context) {
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_RENDER_QUALITY);
    if (bongo_cat_pref_toggle(context, "large-render-optimization", tr(app,
        "pages.preference.general.labels.largeRenderOptimization", "Large Rendering Optimization"),
        tr(app, "pages.preference.general.hints.largeRenderOptimization",
            "Request parallel recording, Bindless, async compute and large memory blocks where supported. May use more memory; reloads the renderer and model."),
        &options->large_render_optimization)) app->render_backend_swap_pending = true;
    if (!options->large_render_optimization) return;
    uint32_t active = bongo_cat_model_runtime_optimizations(app->model_runtime);
    if (!active) {
        nk_layout_row_dynamic(context, 36, 1);
        nk_label_wrap(context, tr(app, "pages.preference.general.hints.largeRenderUnavailable",
            "The current model renderer provides none of these optimizations."));
        return;
    }
    const uint32_t bits[] = {BONGO_CAT_OPTIMIZE_PARALLEL_RECORDING, BONGO_CAT_OPTIMIZE_BINDLESS,
        BONGO_CAT_OPTIMIZE_ASYNC_COMPUTE, BONGO_CAT_OPTIMIZE_LARGE_ALLOCATION};
    const char *keys[] = {"pages.preference.general.options.parallelRecording",
        "pages.preference.general.options.bindless", "pages.preference.general.options.asyncCompute",
        "pages.preference.general.options.largeAllocation"};
    const char *fallbacks[] = {"Parallel command recording", "Bindless", "Async compute", "Large memory blocks"};
    for (size_t i = 0; i < SDL_arraysize(bits); ++i) {
        char status[256];
        snprintf(status, sizeof(status), "%s: %s", tr(app, keys[i], fallbacks[i]),
            tr(app, active & bits[i] ? "pages.preference.general.options.optimizationActive" :
                "pages.preference.general.options.optimizationUnavailable",
                active & bits[i] ? "Enabled" : "Unavailable"));
        nk_layout_row_dynamic(context, 24, 1);
        nk_label(context, status, NK_TEXT_LEFT);
    }
}
