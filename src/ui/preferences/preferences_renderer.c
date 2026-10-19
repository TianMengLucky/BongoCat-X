#include "preferences_internal.h"
#include "preferences_state.h"
#include "preferences_widgets.h"
#include "preferences_widgets_internal.h"
#include "preferences_notice.h"
#include "bongo_cat/i18n.h"
#include "bongo_cat/safe_ffi.h"
#include "bongo_cat/model_plugin.h"
#include "runtime.h"
#include <stdio.h>

static const char *tr(BongoCatApp *app, const char *key, const char *fallback) {
    return bongo_cat_i18n_get(app->i18n, key, fallback);
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
            "Switch immediately; failure restores OpenGL. Experimental Vulkan/Metal also support backgrounds, corners and key/effect/pointer overlays."),
        items, count, selected);
    if (values[next] != options->render_backend) {
        options->render_backend = values[next];
        app->render_backend_swap_pending = true;
        bongo_cat_preferences_notice_show(app, tr(app,
            "pages.preference.general.hints.renderBackendChanged",
            "Switching the render backend"), false);
    }
#ifdef _WIN32
    {
        bool composition = bongo_safe_direct_composition_get(app->settings.extensions_json,
            sizeof(app->settings.extensions_json));
        bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_RENDER_QUALITY);
        if (bongo_cat_pref_toggle(context, "direct-composition", tr(app,
                "pages.preference.general.labels.directComposition", "DirectComposition presentation"),
                tr(app, "pages.preference.general.hints.directComposition",
                    "Experimental GPU-only DXGI alpha presentation. Available on all GPU brands with driver interop support. Uses OpenGL, full mouse click-through and no VSync, without expose throttling. Failure restores direct OpenGL. Game Independent Flip is not guaranteed."),
                &composition)) {
            if (bongo_safe_direct_composition_set(app->settings.extensions_json,
                    sizeof(app->settings.extensions_json), composition))
                app->render_backend_swap_pending = true;
            else bongo_cat_preferences_notice_show(app, tr(app,
                "pages.preference.general.hints.glTrueTransparencySaveFailed",
                "Unable to save the transparency preference"), true);
        }
    }
    if (bongo_cat_rhi_is_gl(&app->rhi)) {
        bool enabled = bongo_safe_gl_true_transparency_get(app->settings.extensions_json,
            sizeof(app->settings.extensions_json));
        bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_RENDER_QUALITY);
        if (bongo_cat_pref_toggle(context, "gl-true-transparency", tr(app,
                "pages.preference.general.labels.glTrueTransparency", "True transparency"),
                tr(app, "pages.preference.general.hints.glTrueTransparency",
                    "Use per-pixel alpha composition like Vulkan. Applies immediately; pixel readback may increase CPU/GPU usage."),
                &enabled)) {
            if (bongo_safe_gl_true_transparency_set(app->settings.extensions_json,
                    sizeof(app->settings.extensions_json), enabled))
                app->render_backend_swap_pending = true;
            else bongo_cat_preferences_notice_show(app, tr(app,
                "pages.preference.general.hints.glTrueTransparencySaveFailed",
                "Unable to save the transparency preference"), true);
        }
    }
#endif
}

void bongo_cat_preferences_large_render_row(BongoCatApp *app,
    BongoCatApplicationPreferences *options, struct nk_context *context) {
    bool metal = app->rhi.backend == BONGO_CAT_RHI_METAL;
    if (app->rhi.backend != BONGO_CAT_RHI_VULKAN && !metal) return;
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_RENDER_QUALITY);
    if (bongo_cat_pref_toggle(context, "large-render-optimization", tr(app,
        "pages.preference.general.labels.largeRenderOptimization", "Large Rendering Optimization"),
        tr(app, "pages.preference.general.hints.largeRenderOptimization",
            "Choose optional Vulkan/Metal optimizations. Changes reload the model; memory use may increase and support depends on the renderer."),
        &options->large_render_optimization)) app->render_backend_swap_pending = true;
    if (!options->large_render_optimization) return;
    uint32_t active = bongo_cat_model_runtime_optimizations(app->model_runtime);
    const uint32_t bits[] = {BONGO_CAT_OPTIMIZE_PARALLEL_RECORDING, BONGO_CAT_OPTIMIZE_BINDLESS,
        BONGO_CAT_OPTIMIZE_LARGE_ALLOCATION};
    const char *keys[] = {"pages.preference.general.options.parallelRecording",
        "pages.preference.general.options.bindless", "pages.preference.general.options.largeAllocation"};
    const char *fallbacks[] = {"Parallel command recording", "Bindless", "Large memory blocks"};
    const char *ids[] = {"render-parallel-recording", "render-bindless",
        "render-large-allocation"};
    bool *values[] = {&options->render_parallel_recording, &options->render_bindless,
        &options->render_large_allocation};
    const char *hint_keys[] = {"pages.preference.general.hints.parallelRecording",
        "pages.preference.general.hints.bindless", "pages.preference.general.hints.largeAllocation"};
    const char *hint_fallbacks[] = {
        "Record Vulkan commands on several threads when the model and renderer support it.",
        "Use a Vulkan texture table when supported. Live2D has model/device limits; Inox2D does not implement this option.",
        "Pool Vulkan resources or share Inox2D Metal vertex buffers. Live2D Metal handles buffer reuse automatically."};
    char descriptions[3][1024];
    float detail_width = NK_MAX(1.0f,
        (nk_window_get_content_region(context).w - context->style.window.spacing.x) * .96f -
        2 * (context->style.window.group_padding.x + context->style.window.group_border) - 26.0f - 33.0f);
    /* Inset the child group so these controls read as master-switch children. */
    size_t first = metal ? 2 : 0;
    float height = 2 * (context->style.window.group_padding.y + context->style.window.group_border);
    for (size_t i = first; i < SDL_arraysize(bits); ++i) {
        const char *status = !*values[i] ? tr(app,
            "pages.preference.general.options.optimizationDisabled", "Disabled") :
            tr(app, active & bits[i] ? "pages.preference.general.options.optimizationActive" :
                "pages.preference.general.options.optimizationUnavailable",
                active & bits[i] ? "Enabled" : "Unavailable");
        snprintf(descriptions[i], sizeof(descriptions[i]), "%s\n%s",
            tr(app, hint_keys[i], hint_fallbacks[i]), status);
        int lines = bongo_cat_pref_detail_text(context, descriptions[i], nk_rect(0, 0, detail_width, 0), false);
        height += 70.0f + 19.0f * lines + context->style.window.spacing.y;
    }
    nk_layout_row_begin(context, NK_DYNAMIC, height, 2);
    nk_layout_row_push(context, .04f);
    nk_spacing(context, 1);
    nk_layout_row_push(context, .96f);
    if (nk_group_begin(context, "large-render-options", NK_WINDOW_NO_SCROLLBAR)) {
        for (size_t i = first; i < SDL_arraysize(bits); ++i) {
            if (bongo_cat_pref_toggle(context, ids[i], tr(app, keys[i], fallbacks[i]),
                    descriptions[i], values[i])) app->render_backend_swap_pending = true;
        }
        nk_group_end(context);
    }
    nk_layout_row_end(context);
}
