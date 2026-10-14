#include "bongo_cat/app.h"
#include "bongo_cat/statistics.h"
#include "bongo_cat/safe_ffi.h"
#include "bongo_cat/i18n.h"
#include "ui_backend.h"
#include "ui_font.h"
#include "ui_present.h"
#include <SDL3/SDL_opengl.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef _WIN32
#include "windows_hdr.h"
#endif

typedef struct BongoCatStatisticsOverlay {
    SDL_Window *window;
    SDL_GLContext context;
    BongoCatUIBackend ui;
    BongoCatLanguage language;
    uint64_t next_frame_ns;
    int x, y;
} BongoCatStatisticsOverlay;

static const char *label(BongoCatApp *app, const char *key, const char *fallback) {
    char full[128];
    snprintf(full, sizeof(full), "pages.preference.statistics.%s", key);
    return bongo_cat_i18n_get(app->i18n, full, fallback);
}

void bongo_cat_statistics_overlay_destroy(BongoCatApp *app) {
    BongoCatStatisticsOverlay *v = app->statistics_overlay;
    if (!v) return;
    SDL_Window *previous = SDL_GL_GetCurrentWindow();
    SDL_GLContext context = SDL_GL_GetCurrentContext();
    if (previous == v->window) { previous = NULL; context = NULL; }
    if (v->context && SDL_GL_MakeCurrent(v->window, v->context))
        bongo_cat_ui_destroy(&v->ui);
    else bongo_cat_ui_abandon(&v->ui);
    if (v->context) SDL_GL_DestroyContext(v->context);
    SDL_DestroyWindow(v->window);
    free(v);
    app->statistics_overlay = NULL;
    if (previous && context) SDL_GL_MakeCurrent(previous, context);
    else SDL_GL_MakeCurrent(NULL, NULL);
}

static bool create(BongoCatApp *app) {
    BongoCatStatisticsOverlay *v = calloc(1, sizeof(*v));
    if (!v) return false;
    app->statistics_overlay = v;
    v->language = app->settings.app.language;
    v->x = v->y = SDL_WINDOWPOS_UNDEFINED;
    v->window = SDL_CreateWindow(label(app, "title", "Statistics"), 360, 150,
        SDL_WINDOW_OPENGL | SDL_WINDOW_TRANSPARENT | SDL_WINDOW_BORDERLESS |
        SDL_WINDOW_UTILITY | SDL_WINDOW_NOT_FOCUSABLE | SDL_WINDOW_HIDDEN |
        SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!v->window) return false;
#ifdef _WIN32
    bongo_cat_windows_prepare_transparent_ui(v->window);
#endif
    int sharing = 0;
    SDL_GL_GetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, &sharing);
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0);
    v->context = SDL_GL_CreateContext(v->window);
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, sharing);
    if (!v->context || !SDL_GL_MakeCurrent(v->window, v->context)) return false;
    SDL_GL_SetSwapInterval(0);
    char body[BONGO_CAT_PATH_CAP], fallback[BONGO_CAT_PATH_CAP], korean[BONGO_CAT_PATH_CAP];
    const char *font = bongo_cat_ui_system_font(body, sizeof(body), false);
    const char *cjk = bongo_cat_ui_system_font(fallback, sizeof(fallback), true);
    const char *kr = bongo_cat_ui_system_korean_font(korean, sizeof(korean));
    nk_rune ranges[8192] = {0};
    bongo_cat_i18n_glyph_ranges(app->i18n, ranges, sizeof(ranges) / sizeof(ranges[0]));
    float layout = 1, raster = 1;
    bongo_cat_ui_query_window_scale(v->window, &layout, &raster);
    BongoCatError error = {0};
    return bongo_cat_ui_init(&v->ui, v->window, font, cjk, kr, font, cjk, kr,
        ranges, ranges, 1, raster, &error);
}

static bool paint(BongoCatApp *app, const BongoCatStatisticsMetrics *m) {
    BongoCatStatisticsOverlay *v = app->statistics_overlay;
    if (!SDL_GL_MakeCurrent(v->window, v->context)) return false;
    int width, height;
    SDL_GetWindowSizeInPixels(v->window, &width, &height);
    glViewport(0, 0, width, height);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    struct nk_context *ctx = &v->ui.context;
    ctx->style.window.fixed_background = nk_style_item_color(nk_rgba(190, 225, 250, 100));
    ctx->style.window.padding = nk_vec2(14, 10);
    ctx->style.window.background = nk_rgba(190, 225, 250, 100);
    ctx->style.window.border = 0;
    ctx->style.window.rounding = 14;
    ctx->style.text.color = nk_rgb(32, 68, 100);
    bongo_cat_ui_input_begin(&v->ui);
    bongo_cat_ui_input_end(&v->ui);
    if (nk_begin(ctx, "statistics-overlay", nk_rect(0, 0, 360, 150), NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_dynamic(ctx, 22, 1);
        nk_label(ctx, label(app, "today", "Today"), NK_TEXT_LEFT);
        uint64_t clicks = 0;
        for (unsigned i = 0; i < BONGO_CAT_STATISTICS_BUTTONS; ++i)
            clicks = UINT64_MAX - clicks < m->buttons[i] ? UINT64_MAX : clicks + m->buttons[i];
        const char *names[] = {label(app, "keys", "Key presses"), label(app, "clicks", "Mouse clicks"),
            label(app, "overlayMovement", "Pointer travel"), label(app, "overlayScroll", "Scroll")};
        char numbers[4][48];
        snprintf(numbers[0], sizeof(numbers[0]), "%llu", (unsigned long long)m->keys);
        snprintf(numbers[1], sizeof(numbers[1]), "%llu", (unsigned long long)clicks);
        snprintf(numbers[2], sizeof(numbers[2]), "%.0f", m->movement);
        snprintf(numbers[3], sizeof(numbers[3]), "%.1f", m->scroll);
        for (unsigned i = 0; i < 4; ++i) {
            nk_layout_row_begin(ctx, NK_DYNAMIC, 22, 2);
            nk_layout_row_push(ctx, .72f); nk_label(ctx, names[i], NK_TEXT_LEFT);
            nk_layout_row_push(ctx, .28f); nk_label(ctx, numbers[i], NK_TEXT_RIGHT);
            nk_layout_row_end(ctx);
        }
    }
    nk_end(ctx);
    return bongo_cat_ui_render(&v->ui) && bongo_cat_ui_present(v->window);
}

void bongo_cat_statistics_overlay_update(BongoCatApp *app) {
    if (!app || !app->window) return;
    bool enabled = bongo_safe_statistics_overlay_get(app->settings.extensions_json,
        sizeof(app->settings.extensions_json));
    if (!enabled || !app->statistics) {
        app->statistics_overlay_retry_ns = 0;
        bongo_cat_statistics_overlay_destroy(app);
        return;
    }
    uint64_t now = SDL_GetTicksNS();
    if (app->statistics_overlay && app->statistics_overlay->language != app->settings.app.language)
        bongo_cat_statistics_overlay_destroy(app);
    bool visible = app->session.window.visible && !app->window_minimized &&
        !app->hover_hidden && !app->startup_visibility_pending && !app->settings.window.capture_only;
    if (!visible) {
        if (app->statistics_overlay) SDL_HideWindow(app->statistics_overlay->window);
        return;
    }
    SDL_Window *previous = SDL_GL_GetCurrentWindow();
    SDL_GLContext context = SDL_GL_GetCurrentContext();
    if (!app->statistics_overlay) {
        if (now < app->statistics_overlay_retry_ns) return;
        if (!create(app)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO, "Cannot create statistics overlay: %s", SDL_GetError());
            bongo_cat_statistics_overlay_destroy(app);
            app->statistics_overlay_retry_ns = now + 5000000000ull;
            goto restore;
        }
    }
    BongoCatStatisticsOverlay *v = app->statistics_overlay;
    int x, y, width;
    if (!SDL_GetWindowPosition(app->window, &x, &y) ||
        !SDL_GetWindowSize(app->window, &width, NULL)) goto restore;
    x += (width - 360) / 2;
    y -= 158;
    SDL_Rect bounds;
    if (SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(app->window), &bounds)) {
        x = SDL_clamp(x, bounds.x, SDL_max(bounds.x, bounds.x + bounds.w - 360));
        y = SDL_clamp(y, bounds.y, SDL_max(bounds.y, bounds.y + bounds.h - 150));
    }
    if (x != v->x || y != v->y) {
        SDL_SetWindowPosition(v->window, x, y); v->x = x; v->y = y;
    }
    bool always_on_top = (SDL_GetWindowFlags(v->window) & SDL_WINDOW_ALWAYS_ON_TOP) != 0;
    if (always_on_top != app->settings.window.always_on_top)
        SDL_SetWindowAlwaysOnTop(v->window, app->settings.window.always_on_top);
    if (now >= v->next_frame_ns || (SDL_GetWindowFlags(v->window) & SDL_WINDOW_HIDDEN)) {
        BongoCatStatisticsSnapshot snapshot;
        if (bongo_cat_statistics_snapshot(app, &snapshot) && paint(app, &snapshot.today)) {
            SDL_ShowWindow(v->window);
            v->next_frame_ns = now + 500000000ull;
        } else {
            bongo_cat_statistics_overlay_destroy(app);
            app->statistics_overlay_retry_ns = now + 5000000000ull;
        }
    }
restore:
    if (previous && context) SDL_GL_MakeCurrent(previous, context);
    else SDL_GL_MakeCurrent(NULL, NULL);
}
