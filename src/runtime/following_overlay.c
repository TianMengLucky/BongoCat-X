#include "following_overlay.h"
#include "plugin_overlay.h"
#include "bongo_cat/i18n.h"
#include "ui_backend.h"
#include "ui_font.h"
#include "ui_present.h"
#include "ui_catime.h"
#include "ui_software.h"
#include "preferences_model_glyphs.h"
#include <string.h>
#include <SDL3/SDL_opengl.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef _WIN32
#include "windows_hdr.h"
#endif

/* Window dimensions and content are contributed by the active plugin. */
typedef struct BongoCatFollowingOverlay {
    SDL_Window *window;
    SDL_GLContext context;
    BongoCatUIBackend ui;
    BongoCatLanguage language;
    uint64_t next_frame_ns;
    int x, y;
    const BongoCatPluginOverlay *api;
    void *instance;
    BongoCatPluginOverlaySurface surface;
    uint64_t names_hash;
} BongoCatFollowingOverlay;

void bongo_cat_following_overlay_destroy(BongoCatFollowingOverlay **slot) {
    BongoCatFollowingOverlay *v = *slot;
    if (!v) return;
    SDL_Window *previous = SDL_GL_GetCurrentWindow();
    SDL_GLContext context = SDL_GL_GetCurrentContext();
    if (previous == v->window) { previous = NULL; context = NULL; }
    if (v->context && SDL_GL_MakeCurrent(v->window, v->context)) {
        bongo_cat_plugin_overlay_release(&v->surface);
        bongo_cat_ui_destroy(&v->ui);
    }
    else bongo_cat_ui_abandon(&v->ui);
    if (v->context) SDL_GL_DestroyContext(v->context);
    SDL_DestroyWindow(v->window);
    free(v);
    *slot = NULL;
    if (previous && context) SDL_GL_MakeCurrent(previous, context);
    else SDL_GL_MakeCurrent(NULL, NULL);
}

static bool create(BongoCatApp *app, BongoCatFollowingOverlay **slot,
    const BongoCatPluginOverlay *api, void *instance) {
    BongoCatFollowingOverlay *v = calloc(1, sizeof(*v));
    if (!v) return false;
    *slot = v;
    v->language = app->settings.app.language;
    v->x = v->y = SDL_WINDOWPOS_UNDEFINED;
    v->api = api; v->instance = instance;
    v->surface.ui = &v->ui; v->surface.i18n = app->i18n;
    v->window = SDL_CreateWindow(bongo_cat_i18n_get(app->i18n, v->api->title_key, v->api->title),
        (int)v->api->width, (int)v->api->height,
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
    BongoCatPluginOverlayGlyphs glyphs = {ranges, SDL_arraysize(ranges), 0};
    (void)v->api->visit_text(v->instance, bongo_cat_plugin_overlay_text, &glyphs);
    v->names_hash = glyphs.hash;
    float layout = 1, raster = 1;
    bongo_cat_ui_query_window_scale(v->window, &layout, &raster);
    BongoCatError error = {0};
    return bongo_cat_ui_init(&v->ui, v->window, font, cjk, kr, font, cjk, kr,
        ranges, ranges, 1, raster, &error);
}

bool bongo_cat_following_overlay_event(BongoCatFollowingOverlay *v, const SDL_Event *event) {
    if (!v || !event || SDL_GetWindowFromEvent(event) != v->window) return false;
    if ((event->type == SDL_EVENT_MOUSE_BUTTON_DOWN || event->type == SDL_EVENT_MOUSE_BUTTON_UP) &&
        event->button.button == SDL_BUTTON_LEFT) {
        v->api->pointer(v->instance, event->button.x, event->button.y, event->button.down);
        v->next_frame_ns = 0;
    }
    return true;
}

static bool paint(BongoCatApp *app, BongoCatFollowingOverlay *v) {
    if (!SDL_GL_MakeCurrent(v->window, v->context)) return false;
    int width, height;
    SDL_GetWindowSizeInPixels(v->window, &width, &height);
    glViewport(0, 0, width, height);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    bool dark = app->settings.app.theme == BONGO_CAT_THEME_DARK ||
        (app->settings.app.theme == BONGO_CAT_THEME_AUTO && SDL_GetSystemTheme() == SDL_SYSTEM_THEME_DARK);
    v->surface.dark = dark;
    struct nk_context *ctx = &v->ui.context;
    ctx->style.window.fixed_background = nk_style_item_color(nk_rgba(0, 0, 0, 0));
    ctx->style.window.padding = nk_vec2(0, 0);
    ctx->style.window.background = nk_rgba(0, 0, 0, 0);
    ctx->style.window.border = 0;
    bongo_cat_ui_input_begin(&v->ui);
    bongo_cat_ui_input_end(&v->ui);
    bool drawn = false;
    if (nk_begin(ctx, "plugin-overlay", nk_rect(0, 0, (float)v->api->width, (float)v->api->height), NK_WINDOW_NO_SCROLLBAR)) {
        BongoCatPluginOverlayCanvas canvas = bongo_cat_plugin_overlay_canvas(&v->surface, app->data_root);
        drawn = v->api->draw(v->instance, &canvas) != 0;
    }
    nk_end(ctx);
    return drawn && bongo_cat_ui_render(&v->ui) && bongo_cat_ui_present(v->window);
}

static bool refresh_application_fonts(BongoCatApp *app, BongoCatFollowingOverlay **slot, uint64_t now, uint64_t *retry) {
    BongoCatFollowingOverlay *v = *slot;
    BongoCatPluginOverlayGlyphs glyphs = {0};
    bool active = v->api->visit_text(v->instance, bongo_cat_plugin_overlay_text, &glyphs) != 0;
    if (!active || v->names_hash == glyphs.hash) return false;
    const BongoCatPluginOverlay *api = v->api;
    void *instance = v->instance;
    bongo_cat_following_overlay_destroy(slot);
    if (!create(app, slot, api, instance)) {
        bongo_cat_following_overlay_destroy(slot);
        *retry = now + 5000000000ull;
    }
    /* Position and topmost state are applied before the new window is shown. */
    return true;
}

void bongo_cat_following_overlay_update(BongoCatApp *app, BongoCatFollowingOverlay **slot,
    const BongoCatPluginOverlay *api, void *instance, bool enabled, bool left, uint64_t *retry) {
    if (!app || !app->window) return;
    if (!enabled || !api || !instance) {
        *retry = 0;
        bongo_cat_following_overlay_destroy(slot);
        return;
    }
    uint64_t now = SDL_GetTicksNS();
    if ((*slot) && (*slot)->language != app->settings.app.language)
        bongo_cat_following_overlay_destroy(slot);
    bool visible = app->session.window.visible && !app->window_minimized &&
        !app->hover_hidden && !app->startup_visibility_pending && !app->settings.window.capture_only;
    if (!visible) {
        if ((*slot)) SDL_HideWindow((*slot)->window);
        return;
    }
    SDL_Window *previous = SDL_GL_GetCurrentWindow();
    SDL_GLContext context = SDL_GL_GetCurrentContext();
    if (!(*slot)) {
        if (now < *retry) return;
        if (!create(app, slot, api, instance)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO, "Cannot create plugin overlay: %s", SDL_GetError());
            bongo_cat_following_overlay_destroy(slot);
            *retry = now + 5000000000ull;
            goto restore;
        }
    }
    BongoCatFollowingOverlay *v = (*slot);
    int x, y, width;
    if (!SDL_GetWindowPosition(app->window, &x, &y) ||
        !SDL_GetWindowSize(app->window, &width, NULL)) goto restore;
    if (left) {
        x -= (int)v->api->width + 8;
    } else {
        x += (width - (int)v->api->width) / 2;
        y -= (int)v->api->height + 8;
    }
    SDL_Rect bounds;
    if (SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(app->window), &bounds)) {
        x = SDL_clamp(x, bounds.x, SDL_max(bounds.x, bounds.x + bounds.w - (int)v->api->width));
        y = SDL_clamp(y, bounds.y, SDL_max(bounds.y, bounds.y + bounds.h - (int)v->api->height));
    }
    if (x != v->x || y != v->y) {
        SDL_SetWindowPosition(v->window, x, y); v->x = x; v->y = y;
    }
    bool always_on_top = (SDL_GetWindowFlags(v->window) & SDL_WINDOW_ALWAYS_ON_TOP) != 0;
    if (always_on_top != app->settings.window.always_on_top)
        SDL_SetWindowAlwaysOnTop(v->window, app->settings.window.always_on_top);
    if (now >= v->next_frame_ns || (SDL_GetWindowFlags(v->window) & SDL_WINDOW_HIDDEN)) {
        if (refresh_application_fonts(app, slot, now, retry)) goto restore;
        if (paint(app, v)) {
            SDL_ShowWindow(v->window);
            v->next_frame_ns = now + 500000000ull;
        } else {
            bongo_cat_following_overlay_destroy(slot);
            *retry = now + 5000000000ull;
        }
    }
restore:
    if (previous && context) SDL_GL_MakeCurrent(previous, context);
    else SDL_GL_MakeCurrent(NULL, NULL);
}
