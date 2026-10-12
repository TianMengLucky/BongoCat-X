#include "bongo_cat/log.h"
#include "ui_software.h"
#include "preferences_gl.h"
#include "preferences_state.h"
#include "preferences_render_internal.h"
#include "preferences_controls.h"
#include "preferences_model_glyphs.h"
#include "preferences_model_cover.h"
#include "ui_animation.h"
#include "ui_paint.h"
#include "ui_present.h"
#include "bongo_cat/memory_policy.h"

#include <SDL3/SDL_opengl.h>

float bongo_cat_preferences_animation_delta(uint64_t previous_ns, uint64_t now_ns) {
    if (!previous_ns || now_ns <= previous_ns) return 1.0f / 60.0f;
    // Catch up after a slow frame, but bound large idle/startup time gaps.
    return NK_CLAMP(.001f, (float)(now_ns-previous_ns)/1000000000.0f, .05f);
}

static void finish_frame(BongoCatPreferences *value, bool rendered, uint64_t now,
    bool loading_model, bool importing) {
    bongo_cat_preferences_smoke_frame(value);
    if (!rendered) {
        /* Keep the last complete front buffer instead of presenting corruption. */
        value->render_dirty = true;
        value->render_retry_ns = now + 1000000000ull;
    } else if (!bongo_cat_ui_present(value->window)) {
        value->render_dirty = true;
        value->render_retry_ns = now + 1000000000ull;
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
            "Preferences frame presentation failed: %s", SDL_GetError());
    } else if (!loading_model) bongo_cat_memory_policy_ui_frame_presented();
    /* Commands have been consumed; page-only textures can now be released. */
    if (value->page != 4 && (value->about.contributors ||
            value->about.contributors_attempted || value->qq_texture)) {
        bongo_cat_about_clear(value, true);
        if (value->qq_texture) bongo_cat_ui_texture_delete(&value->ui, 1, &value->qq_texture);
        value->qq_texture = 0; value->qq_attempted = false;
        bongo_cat_memory_policy_ui_loaded();
        bongo_cat_memory_policy_ui_frame_presented();
    }
    if (value->page != 1) bongo_cat_preferences_model_cover_cache_clear(value->app);
    bongo_cat_preferences_record_frame(value);
    if (value->shortcut_recording || value->model_load_visual_active ||
        importing || bongo_cat_pref_controls_animating(&value->ui.context) ||
        bongo_cat_ui_animations_active(&value->ui.context))
        value->render_dirty = true;
    else if (!value->chrome_dragging && !value->live_resize_active)
        bongo_cat_ui_trim_idle(&value->ui);
    if ((!value->logo_texture || !value->icon_texture) &&
        value->asset_retry_count < 3) {
        value->render_dirty = true;
        value->render_retry_ns = value->asset_retry_ns;
    }
    bongo_cat_preferences_gl_restore_main(value);
    bongo_cat_ui_cursor_apply(&value->ui);
    if (value->import_requested && !bongo_cat_preferences_import_is_open(
        value->import_dialog)) {
        value->import_requested = false;
        if (!bongo_cat_preferences_import_open(value->import_dialog,
                value->window))
            value->render_dirty = true;
    }
    /* The Core picker opens after the frame, like the model import dialog. */
    if (value->sdk_import_requested) {
        value->sdk_import_requested = false;
        bongo_cat_preferences_request_sdk_import(value);
    }
}

void bongo_cat_preferences_render(BongoCatPreferences *value) {
    bongo_cat_preferences_release_idle_window(value);
    if (!value || !value->window || !value->visible) return;
    if (bongo_cat_preferences_window_fade_tick(value)) return;
    if (value->ui.software_pending) {
        float width, height;
        bongo_cat_ui_logical_size(&value->ui, &width, &height);
        uint64_t started = SDL_GetTicksNS();
        bool rendered = bongo_cat_ui_software_render(&value->ui, width, height);
        if (SDL_getenv("BONGO_CAT_PROFILE_FRAMES")) {
            static unsigned slices;
            if (++slices <= 5 || slices % 120 == 0) SDL_LogInfo(BONGO_CAT_LOG_LIFECYCLE,
                "[frame-time] settings-slice sample=%u ms=%.2f pending=%d", slices,
                (SDL_GetTicksNS()-started)/1000000.0, value->ui.software_pending);
        }
        if (value->ui.software_pending) return;
        finish_frame(value, rendered, started, value->model_loading ||
            value->app->loading_model[0], value->import_render_active);
        return;
    }
    /* Also cover native resize/expose paths reached by SDL_PumpEvents. Keep
       the main GL context current until the atlas transfer has completed. */
    if (bongo_cat_preferences_model_texture_busy(value)) {
        value->render_dirty = true;
        return;
    }
    bongo_cat_preferences_drag_tick(value);
    uint64_t now = SDL_GetTicksNS();
    /* Repaint the color picker only after the dragged color has settled. */
    if (value->color_picker_pending_ns &&
        now - value->color_picker_pending_ns >= 150000000ull) {
        value->color_picker_pending_ns = 0;
        value->render_dirty = true;
    }
    bool raster_due = value->pending_raster_scale > 0.0f &&
        value->raster_retry_ns <= now;
    if (value->render_retry_ns > now ||
        (!value->render_dirty && value->last_render_ns && !raster_due)) return;
    value->render_dirty = false;
    value->ui.context.delta_time_seconds =
        bongo_cat_preferences_animation_delta(value->last_render_ns,now);
    value->last_render_ns = now;
    bongo_cat_preferences_input_end(value);
    if (!bongo_cat_preferences_gl_make_current(value)) {
        value->render_dirty = true;
        value->render_retry_ns = now + 1000000000ull;
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
            "Preferences GL context could not be activated: %s", SDL_GetError());
        bongo_cat_preferences_gl_restore_main(value);
        return;
    }
    value->render_retry_ns = 0;
    bool loading_model = value->model_loading || value->app->loading_model[0];
    /* A resize ending during handoff defers GL cleanup until this context is
       safe to use again. Do not retain the cached window image indefinitely. */
    if (!loading_model && !value->live_resize_active && value->ui.resize_cache_texture)
        bongo_cat_ui_resize_cache_destroy(&value->ui);
    /* Retry transient image failures, retaining successfully loaded assets. */
    if (!loading_model && (!value->logo_texture || !value->icon_texture) &&
        value->asset_retry_count < 3 && now >= value->asset_retry_ns) {
        value->asset_retry_count++;
        value->asset_retry_ns = now + 1000000000ull;
        bongo_cat_preferences_assets_load(value);
    }
    bool importing = bongo_cat_preferences_import_status(
        value->import_dialog, NULL, NULL, NULL);
    value->import_render_active = importing;
    bool refreshing_models = bongo_cat_app_model_refresh_busy(value->app);
    if (value->behavior_font_serial != value->app->model_selection_serial) {
        value->behavior_font_serial = value->app->model_selection_serial;
        if (!bongo_cat_preferences_behavior_glyphs_ready(value)) {
            value->font_reload_pending = true;
            value->font_reload_defer_once = false;
        }
    }
    if (!loading_model && !importing && !refreshing_models) {
        bongo_cat_preferences_refresh_raster(value);
        bongo_cat_preferences_reload_language(value);
    }
    bool notice_font_reload = false;
    for (size_t i = 0; i < sizeof(value->notices) / sizeof(value->notices[0]); ++i)
        if (value->notices[i].message[0] && value->notices[i].until_ns > now &&
            !bongo_cat_preferences_model_glyphs_ready(value, value->notices[i].message))
            notice_font_reload = true;
    if (!loading_model && value->font_reload_pending &&
        (notice_font_reload || (!importing && !refreshing_models))) {
        if (value->font_reload_defer_once) {
            value->font_reload_defer_once = false;
            value->render_dirty = true;
        } else if (!bongo_cat_preferences_reload_fonts(value)) {
            value->font_reload_pending = false;
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "Deferred preferences font atlas rebuild failed");
        }
    }
    bongo_cat_preferences_apply_theme(value);
    float width = 0.0f, height = 0.0f;
    bongo_cat_ui_logical_size(&value->ui, &width, &height);
    bongo_cat_ui_paint_begin_frame(&value->ui);
    bongo_cat_ui_cursor_begin(&value->ui);
    bool dark = bongo_cat_preferences_resolved_theme(value) != 0;
    value->ui.frame_building = true;
    bool close_requested = bongo_cat_preferences_draw_frame(
        value, width, height, dark);
    value->ui.frame_building = false;
    if (close_requested) {
        nk_clear(&value->ui.context);
        bongo_cat_preferences_gl_restore_main(value);
        bongo_cat_preferences_close(value);
        return;
    }
    bongo_cat_preferences_shortcut_smoke(value);
    BongoCatUIPalette palette = bongo_cat_ui_palette(dark);
    if (value->ui.software_renderer) {
        SDL_SetRenderDrawColor(value->ui.software_renderer, palette.background.r, palette.background.g, palette.background.b, 255);
        SDL_RenderClear(value->ui.software_renderer);
    } else {
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(
        value->transparent_window ? 0.0f : palette.background.r / 255.0f,
        value->transparent_window ? 0.0f : palette.background.g / 255.0f,
        value->transparent_window ? 0.0f : palette.background.b / 255.0f,
        value->transparent_window ? 0.0f : 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    }
    bool rendered = bongo_cat_ui_render(&value->ui) &&
        value->ui.last_draw_elements && value->ui.nonzero_alpha_vertices;
    if (value->ui.software_pending) {
        // Preserve newly arriving input across chunks; previous layout input is consumed.
        bongo_cat_preferences_input_begin(value);
        bongo_cat_preferences_gl_restore_main(value);
        return;
    }
    finish_frame(value, rendered, now, loading_model, importing);
}
