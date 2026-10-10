#include "preferences_internal.h"
#include "preferences_state.h"
#include "preferences_theme.h"
#include "preferences_controls.h"
#include "preferences_widgets.h"
#include "preferences_notice.h"
#include "ui_tooltip.h"
#include "bongo_cat/audio.h"
#include "bongo_cat/i18n.h"
#include "bongo_cat/log.h"
#include "bongo_cat/preferences.h"
#include "bongo_cat/tray.h"
#include "runtime.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

static const char *tr(BongoCatApp *app, const char *key, const char *fallback) {
    return bongo_cat_i18n_get(app->i18n, key, fallback);
}

static void section_gap(struct nk_context *context, float pixels) {
    context->current->layout->at_y += pixels;
}

static void page_display(BongoCatPreferences *value, struct nk_context *context) {
    BongoCatApp *app = value->app;
    BongoCatModelPreferences *model = &app->settings.model;
    BongoCatWindowPreferences *window = &app->settings.window;
    BongoCatWindowState *window_state = &app->session.window;
    bongo_cat_pref_section_icon(context, tr(app,
        "pages.preference.cat.labels.windowSettings",
        "Window"),
        BONGO_CAT_PREF_ICON_SECTION_WINDOW);
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_PASS_THROUGH);
    if (bongo_cat_pref_toggle(context, "pass-through", tr(app,
        "composables.useAppMenu.labels.passThrough", "Pass Through"), tr(app,
        "pages.preference.cat.hints.passThrough",
        "Let clicks reach windows behind the pet. Change this from the tray menu if needed."),
        &window->pass_through)) {
        bongo_cat_window_mark_hit_dirty(app);
        bongo_cat_window_sync_click_through(app);
    }
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_ALWAYS_ON_TOP);
    if (bongo_cat_pref_toggle(context, "always-top", tr(app,
        "composables.useAppMenu.labels.alwaysOnTop", "Always on Top"), tr(app, "pages.preference.cat.hints.alwaysOnTop",
            "Keep the pet above other windows."),
        &window->always_on_top)) {
        bongo_cat_platform_set_always_on_top(&app->platform, window->always_on_top);
        bongo_cat_window_mark_hit_dirty(app);
        bongo_cat_window_sync_click_through(app);
    }
    if (app->platform.hover_hide_unavailable) {
        bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_SHORTCUT_VISIBILITY);
        bongo_cat_pref_status(context, "hover-unavailable", tr(app,
            "pages.preference.cat.labels.hideOnHover", "Hide on Hover"), tr(app,
            "pages.preference.cat.hints.hoverUnavailable",
            "Hover hiding is unavailable in the current desktop environment."));
    } else {
        bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_SHORTCUT_VISIBILITY);
        bool hover_available = window->pass_through && window->always_on_top;
        bool hide_on_hover = window->hide_on_hover && hover_available;
        if (bongo_cat_pref_toggle_float_detail(context, "hide-on-hover", tr(app,
            "pages.preference.cat.labels.hideOnHover", "Hide on Hover"),
            tr(app, "pages.preference.cat.labels.secondsUnit", "s"),
            &hide_on_hover, 0.0f, &window->hide_fade_seconds,
            BONGO_CAT_MAX_HIDE_FADE_SECONDS, 0.1f,
            BONGO_CAT_DEFAULT_HIDE_FADE_SECONDS, tr(app,
                "pages.preference.cat.hints.hoverRequires",
                "Hide on hover and reappear when the pointer leaves. Set the fade time; requires [Pass Through] and [Always on Top]."),
            hover_available)) {
            window->hide_on_hover = hide_on_hover;
            bongo_cat_app_update_hover(app, SDL_GetTicksNS());
        }
    }
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_FRAME_BOUNDS);
    bongo_cat_pref_toggle(context, "edge-snap", tr(app,
        "pages.preference.cat.labels.edgeSnap", "Edge Snapping"), tr(app, "pages.preference.cat.hints.edgeSnap",
            "Snap to a screen edge when dragging near it."),
        &window->edge_snap);
    /* 只在录屏/直播软件里显示: 仅 Windows 有等价机制 (DWM 隐藏), 其它平台不显示
       这一项, 免得给用户一个点了没反应的开关。 */
    if (bongo_cat_platform_capture_only_supported()) {
        bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_SOLID_BACKGROUND);
        if (bongo_cat_pref_toggle(context, "capture-only", tr(app,
            "pages.preference.cat.labels.captureOnly", "Capture Only"), tr(app,
            "pages.preference.cat.hints.captureOnly", "Hide the pet on the Windows desktop while keeping it visible to compatible recording software."), &window->capture_only)) {
            bongo_cat_window_apply_capture_only(app);
            app->dirty = true;
        }
    }
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_SOLID_BACKGROUND);
    if (bongo_cat_pref_obs_background(context, "obs-background", tr(app,
        "pages.preference.cat.labels.obsBackground", "Solid Background"), tr(app,
        "pages.preference.cat.hints.obsBackground", "Add a solid color behind the pet. For a black OBS capture, see the instructions here."), tr(app,
        "pages.preference.cat.hints.obsBackgroundHelp", "OBS: enable this option, use the Windows 7 compatibility method, and remove the background with a color key filter."),
        &window->obs_background, &window->obs_background_rgb)) {
        if (window->obs_background) window->custom_background = false;
        app->dirty = true;
        bongo_cat_window_mark_hit_dirty(app);
    }
    bongo_cat_preferences_background_row(value, context);
    /* Repaint only after the dragged color settles (present pass); the pet
       itself recolors live. */
    if (window->obs_background && bongo_cat_pref_color_picker(context,
        "obs-background-picker", &window->obs_background_rgb)) {
        app->dirty = true;
        value->color_picker_pending_ns = SDL_GetTicksNS();
    }
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_FRAME_BOUNDS);
    if (bongo_cat_pref_toggle(context, "tight-frame", tr(app,
        "pages.preference.cat.labels.tightFrame", "Tight Frame"), tr(app,
        "pages.preference.cat.hints.tightFrame", "Fit the window to the visible pet and background, reducing transparent margins."), &window->tight_frame)) {
        SDL_LogInfo(BONGO_CAT_LOG_LIFECYCLE, "[tight] toggle -> %d", window->tight_frame ? 1 : 0);
        bongo_cat_model_runtime_set_tight_frame(app->model_runtime, window->tight_frame);
        app->frame_geometry_retry_ns = 0;
        app->dirty = true;
    }
    float old_scale = window_state->scale_percent;
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_WINDOW_SIZE);
    bool restore_default = false;
    bool reset_position = bongo_cat_pref_float_action_default(context, "window-size",
        tr(app, "pages.preference.cat.labels.windowSize", "Window Size"), tr(app,
        "pages.preference.cat.hints.windowSize", "Resize with the wheel or right-button drag. Reset moves the pet back on screen; the size icon restores the default scale."),
        10.0f, &window_state->scale_percent, 500.0f, 1.0f, BONGO_CAT_DEFAULT_WINDOW_SCALE_PERCENT,
        tr(app, "pages.preference.cat.labels.resetPosition", "Reset"), tr(app,
        "pages.preference.cat.labels.restoreDefaultSize", "Restore Default Size"), &restore_default);
    if (old_scale != window_state->scale_percent && old_scale > 0.0f) {
        float requested_scale = window_state->scale_percent;
        window_state->scale_percent = old_scale;
        bongo_cat_window_cancel_wheel_animation(app);
        bongo_cat_window_set_scale(app, requested_scale);
    }
    if (restore_default && window_state->scale_percent != BONGO_CAT_DEFAULT_WINDOW_SCALE_PERCENT) {
        bongo_cat_window_cancel_wheel_animation(app);
        bongo_cat_window_set_scale(app, BONGO_CAT_DEFAULT_WINDOW_SCALE_PERCENT); }
    if (reset_position) bongo_cat_window_reset_position(app);
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_WINDOW_CORNERS);
    /* Display the fraction of maximum rounding; keep saved radii in their
       original units (percent of the short edge) for existing settings. */
    float corner_roundness = window->corner_radius_percent * 2.0f;
    if (bongo_cat_pref_toggle_float_detail(context, "window-corners", tr(app,
        "pages.preference.cat.labels.windowCorners", "Window Corners"), "%", &window->rounded_corners, 0.0f, &corner_roundness,
        100.0f, 1.0f, BONGO_CAT_DEFAULT_WINDOW_CORNER_PERCENT * 2.0f, tr(app, "pages.preference.cat.hints.windowCorners",
            "Round the window corners. Higher percentages make the corners rounder on all backends."), true)) {
        window->corner_radius_percent = corner_roundness * 0.5f;
        app->dirty = true;
        bongo_cat_window_mark_hit_dirty(app);
    }
    float old_opacity = window_state->opacity_percent;
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_OPACITY);
    bongo_cat_pref_slider(context, "opacity", tr(app,
        "pages.preference.cat.labels.opacity", "Opacity"), tr(app,
        "pages.preference.cat.hints.opacity", "Set window transparency. Ctrl+wheel also adjusts it."),
        10.0f, &window_state->opacity_percent, 100.0f, 1.0f,
        BONGO_CAT_DEFAULT_WINDOW_OPACITY_PERCENT);
    if (old_opacity != window_state->opacity_percent)
        bongo_cat_window_cancel_wheel_animation(app);
    if (old_opacity != window_state->opacity_percent && !app->hover_hidden) {
        bongo_cat_app_cancel_hover_fade(app);
        bongo_cat_platform_set_opacity(&app->platform,
            window_state->opacity_percent / 100.0f);
    }
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_RANDOM_EXPRESSION);
    if (bongo_cat_pref_toggle_float_config(context, "random-expression", tr(app,
        "pages.preference.cat.labels.randomExpression", "Random Expressions"),
        tr(app, "pages.preference.cat.labels.secondsUnit", "s"),
        &window->random_expression, 1.0f, &window->random_expression_interval_seconds,
        3600.0f, 1.0f, BONGO_CAT_DEFAULT_RANDOM_EXPRESSION_SECONDS, tr(app, "pages.preference.cat.hints.randomExpression",
            "Pick a random expression at the set interval. Use the settings button to choose participating expressions.")))
        bongo_cat_preferences_random_dialog_open(value, BONGO_CAT_BEHAVIOR_EXPRESSION);
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_RANDOM_MOTION);
    if (bongo_cat_pref_toggle_float_config(context, "random-motion", tr(app,
        "pages.preference.cat.labels.randomMotion", "Random Motions"),
        tr(app, "pages.preference.cat.labels.secondsUnit", "s"),
        &window->random_motion, 1.0f, &window->random_motion_interval_seconds,
        3600.0f, 1.0f, BONGO_CAT_DEFAULT_RANDOM_MOTION_SECONDS, tr(app, "pages.preference.cat.hints.randomMotion",
            "Play a random motion at the set interval. Choose participating motions with the settings button; each keeps its saved volume.")))
        bongo_cat_preferences_random_dialog_open(value, BONGO_CAT_BEHAVIOR_MOTION);
    bongo_cat_preferences_sequential_model_pref_row(value, context);

    section_gap(context, 10);
    bongo_cat_pref_section_icon(context, tr(app,
        "pages.preference.cat.labels.modelSettings", "Model"),
        BONGO_CAT_PREF_ICON_SECTION_MODEL);
    bongo_cat_preferences_model_hands_rows(app, model, context);
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_MIRROR);
    if (bongo_cat_pref_toggle(context, "mirror", tr(app,
        "pages.preference.cat.labels.mirrorMode", "Mirror Mode"), tr(app, "pages.preference.cat.hints.mirrorMode",
            "Flip the model horizontally. Compatible Mver key characters stay readable."), &model->mirror)) {
        app->model_pointer_anchor_ready = false;
        app->pointer_known = false;
        app->dirty = true;
    }
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_VERTICAL_FLIP);
    if (bongo_cat_pref_toggle(context, "vertical-flip", tr(app,
        "pages.preference.cat.labels.verticalFlip", "Hang Upside Down"), tr(app, "pages.preference.cat.hints.verticalFlip",
            "Flip the model vertically to hang it upside down."),
        &model->vertical_flip))
        bongo_cat_app_reset_pointer_tracking(app);
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_MOUSE_MIRROR);
    if (bongo_cat_pref_toggle(context, "mouse-mirror", tr(app,
        "pages.preference.cat.labels.mouseMirror", "Mouse Horizontal Flip"), tr(app, "pages.preference.cat.hints.mouseMirror",
            "Reverse horizontal mouse tracking without flipping the model."),
        &model->mouse_mirror)) {
        app->pointer_known = false;
        app->dirty = true;
    }
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_MOUSE_VERTICAL_FLIP);
    if (bongo_cat_pref_toggle(context, "mouse-vertical-flip", tr(app,
        "pages.preference.cat.labels.mouseVerticalFlip", "Mouse Vertical Flip"), tr(app, "pages.preference.cat.hints.mouseVerticalFlip",
            "Reverse vertical mouse tracking without flipping the model."),
        &model->mouse_vertical_flip))
        bongo_cat_app_reset_pointer_tracking(app);
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_MOUSE_CENTERED);
    bool disable_mouse_centered = !model->mouse_centered;
    if (bongo_cat_pref_toggle(context, "mouse-centered", tr(app,
        "pages.preference.cat.labels.mouseCentered",
        "Disable Mouse Centering on Desktop Pet"), tr(app, "pages.preference.cat.hints.mouseCentered",
            "Use the screen center instead of the pet as the mouse-tracking reference."), &disable_mouse_centered)) {
        model->mouse_centered = !disable_mouse_centered;
        bongo_cat_app_reset_pointer_tracking(app);
    }
    bongo_cat_preferences_mouse_input_rows(app, model, context);
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_MAX_FPS);
    model->max_fps = bongo_cat_pref_fps(context, "max-fps", tr(app,
        "pages.preference.cat.labels.maxFPS", "Max Frame Rate"), tr(app, "pages.preference.cat.hints.maxFPS",
            "Limit model frame rate. Drag up to the display rate, type up to 360 FPS, or double-click to restore the default."), model->max_fps,
        app->startup_display_fps);
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_RENDER_QUALITY);
    float next_quality = bongo_cat_pref_render_quality(context, "render-quality",
        tr(app, "pages.preference.cat.labels.renderQuality",
            "Model Quality (%)"),
        tr(app, "pages.preference.cat.hints.renderQuality",
            "Lower texture resolution to save video memory. Changes settle before textures reload; double-click restores the default."),
        model->render_quality_percent);
    /* Reload after 300 ms without change; per-frame reloads stutter the UI. */
    static float quality_reload_from = 0.0f;
    static uint64_t quality_settle_deadline_ns = 0;
    if (next_quality != model->render_quality_percent) {
        quality_reload_from = model->render_quality_percent;
        model->render_quality_percent = next_quality;
        quality_settle_deadline_ns = SDL_GetTicksNS() + 300000000ull;
    } else if (quality_settle_deadline_ns &&
        SDL_GetTicksNS() >= quality_settle_deadline_ns) {
        quality_settle_deadline_ns = 0;
        BongoCatError reload_error = {0};
        bool reloaded = !app->loaded_model[0] ||
            bongo_cat_model_runtime_try_reuse_texture_quality(app->model_runtime,
                model->render_quality_percent) ||
            bongo_cat_app_reload_model_with_error(app, &reload_error);
        if (!reloaded) {
            model->render_quality_percent = quality_reload_from;
            char message[1024];
            snprintf(message, sizeof(message), "%s\n%s", tr(app,
                "pages.preference.cat.hints.dynamicTextureResolutionFailed",
                "Unable to reload the model with the selected texture mode."),
                reload_error.message);
            bongo_cat_preferences_notice_show(app, message, true);
        } else app->dirty = true;
    }
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_TEXTURE_RESOLUTION);
    bool old_dynamic_texture_resolution = model->dynamic_texture_resolution;
    bool disable_dynamic_texture_resolution = !model->dynamic_texture_resolution;
    if (bongo_cat_pref_toggle(context, "dynamic-texture-resolution", tr(app,
        "pages.preference.cat.labels.dynamicTextureResolution",
        "Disable Dynamic Texture Resolution"), tr(app,
        "pages.preference.cat.hints.dynamicTextureResolution",
        "When enabled, stop adapting textures to window size and use the quality-selected resolution, which may use more video memory."),
        &disable_dynamic_texture_resolution)) {
        model->dynamic_texture_resolution = !disable_dynamic_texture_resolution;
        SDL_LogInfo(BONGO_CAT_LOG_LIFECYCLE,
            "[memory] texture-mode-change previous=%d requested=%d",
            old_dynamic_texture_resolution, model->dynamic_texture_resolution);
        BongoCatError reload_error = {0};
        bool reloaded = !app->loaded_model[0] ||
            bongo_cat_app_reload_model_with_error(app, &reload_error);
        if (!reloaded) {
            model->dynamic_texture_resolution = old_dynamic_texture_resolution;
            char message[1024];
            snprintf(message, sizeof(message), "%s\n%s", tr(app,
                "pages.preference.cat.hints.dynamicTextureResolutionFailed",
                "Unable to reload the model with the selected texture mode."),
                reload_error.message);
            bongo_cat_preferences_notice_show(app, message, true);
        } else app->dirty = true;
        SDL_LogInfo(BONGO_CAT_LOG_LIFECYCLE,
            "[memory] texture-mode-result reloaded=%d active=%d",
            reloaded, model->dynamic_texture_resolution);
    }
}

void bongo_cat_preferences_page_settings(BongoCatPreferences *value,
    struct nk_context *context) {
    page_display(value, context);
    section_gap(context, 10);
    bongo_cat_preferences_page_general(value->app, context);
}
