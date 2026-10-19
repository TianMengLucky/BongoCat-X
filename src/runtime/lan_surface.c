#include "lan_surface.h"
#include "lan_graphics.h"
#include "lan_native.h"
#include "bongo_cat/i18n.h"
#include "bongo_cat/log.h"
#include "bongo_cat/overlay.h"
#include "mver/mver_render.h"
#include "mver/mver_config.h"
#include "model_import.h"
#include "ui_backend.h"
#include "ui_font.h"
#include "ui_present.h"
#include "preferences_model_glyphs.h"
#include <SDL3/SDL_opengl.h>
#include <string.h>
#ifdef _WIN32
#include "windows_hdr.h"
#endif
#define LAN_WIDTH 480
#define LAN_HEIGHT 320
#define LAN_NAME_HEIGHT 28
const char *const bongo_cat_lan_parameters[BONGO_CAT_LAN_PARAMETERS] = {
    "CatParamLeftHandDown", "CatParamRightHandDown", "ParamMouseX", "ParamMouseY",
    "ParamMouseLeftDown", "ParamMouseRightDown", "ParamAngleX", "ParamAngleY",
    "ParamAngleZ", "ParamBodyAngleX", "ParamEyeBallX", "ParamEyeBallY",
    "CatParamStickLX", "CatParamStickLY", "CatParamStickRX", "CatParamStickRY"
};
struct BongoCatLanSurface {
    BongoCatLanNative *native;
    SDL_Window *window;
    SDL_GLContext context;
    BongoCatModelRuntime *model;
    BongoCatOverlay *overlay;
    BongoCatUIBackend ui;
    bool ui_ready, dragging;
    float drag_x, drag_y;
    uint32_t generation;
    uint64_t frame;
    char name[128];
};
void bongo_cat_lan_surface_destroy(BongoCatLanSurface **slot) {
    BongoCatLanSurface *v = *slot;
    if (!v) return;
    if (v->native) {
        bongo_cat_lan_native_destroy(&v->native); SDL_free(v); *slot = NULL; return;
    }
    SDL_Window *previous = SDL_GL_GetCurrentWindow();
    SDL_GLContext context = SDL_GL_GetCurrentContext();
    if (previous == v->window) { previous = NULL; context = NULL; }
    if (v->context && SDL_GL_MakeCurrent(v->window, v->context)) {
        bongo_cat_model_runtime_destroy(v->model);
        bongo_cat_overlay_destroy(v->overlay);
        if (v->ui_ready) bongo_cat_ui_destroy(&v->ui);
    }
    /* The main pet owns the shared GL context. */
    if (v->window) SDL_DestroyWindow(v->window);
    SDL_free(v); *slot = NULL;
    SDL_GL_MakeCurrent(previous, context);
}
static bool create(BongoCatApp *app, BongoCatLanSurface **slot,
    const BongoCatLanSnapshot *snapshot, const char *name) {
    BongoCatLanSurface *v = SDL_calloc(1, sizeof(*v));
    if (!v) return false;
    *slot = v;
    v->generation = snapshot->generation;
    v->window = SDL_CreateWindow(name, LAN_WIDTH, LAN_HEIGHT,
        SDL_WINDOW_OPENGL | SDL_WINDOW_TRANSPARENT | SDL_WINDOW_BORDERLESS |
        SDL_WINDOW_UTILITY | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!v->window) return false;
#ifdef _WIN32
    bongo_cat_windows_prepare_transparent_ui(v->window);
#endif
    /* Cubism shares context-local offscreen targets between model instances.
       Switch drawables while retaining the main context and its object namespace. */
    v->context = app->gl_context;
    if (!v->context || !SDL_GL_MakeCurrent(v->window, v->context)) return false;
    BongoCatError error = {0};
    v->model = bongo_cat_model_runtime_create(app->asset_root, &error);
    v->overlay = bongo_cat_overlay_create(&error);
    BongoCatModelRuntimeRenderOptions options = {0};
    options.projection_scale = 1.0f;
    char config[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_import_render_options(snapshot->model.adapter, &options) &&
        bongo_cat_mver_config_find(snapshot->model.directory, config, sizeof(config)))
        (void)bongo_cat_mver_render_read(config, &options);
    if (!v->model || !v->overlay || bongo_cat_model_runtime_load(v->model,
        snapshot->model.directory, snapshot->model.setting, false, &options,
        NULL, NULL, &error) != BONGO_CAT_OK) {
        SDL_LogWarn(BONGO_CAT_LOG_LIFECYCLE, "[lan] Cannot render friend model: %s", error.message);
        return false;
    }
    (void)bongo_cat_overlay_load(v->overlay, snapshot->model.adapter, false,
        bongo_cat_model_runtime_rendering(v->model), &options, &error);
    bongo_cat_lan_position(app,v->window,snapshot->peer_id);
    v->ui_ready = bongo_cat_lan_name_init(app, &v->ui, v->window, name);
    if (v->ui_ready) SDL_strlcpy(v->name, name, sizeof(v->name));
    return v->ui_ready;
}
bool bongo_cat_lan_surface_update(BongoCatApp *app, BongoCatLanSurface **slot,
    const BongoCatLanSnapshot *snapshot, const char *name) {
    if (*slot && (((*slot)->native != NULL) != (app->rhi.backend == BONGO_CAT_RHI_VULKAN)))
        bongo_cat_lan_surface_destroy(slot);
    if (app->rhi.backend == BONGO_CAT_RHI_VULKAN) {
        if (!*slot) *slot = SDL_calloc(1, sizeof(**slot));
        if (!*slot) return false;
        bool ready = bongo_cat_lan_native_update(app, &(*slot)->native, snapshot, name);
        (*slot)->window = bongo_cat_lan_native_window((*slot)->native);
        if (!ready) bongo_cat_lan_surface_destroy(slot);
        return ready;
    }
    if (!app->gl_context || !bongo_cat_rhi_is_gl(&app->rhi)) return false;
    SDL_Window *previous = SDL_GL_GetCurrentWindow();
    SDL_GLContext context = SDL_GL_GetCurrentContext();
    bool ok = true;
    if (*slot && (*slot)->generation != snapshot->generation) bongo_cat_lan_surface_destroy(slot);
    if (!*slot) ok = create(app, slot, snapshot, name);
    BongoCatLanSurface *v = *slot;
    if (!ok || !v || !SDL_GL_MakeCurrent(v->window, v->context)) { ok = false; goto cleanup; }
    if (strcmp(v->name, name)) {
        bongo_cat_ui_destroy(&v->ui); v->ui_ready = false;
        v->ui_ready = bongo_cat_lan_name_init(app, &v->ui, v->window, name);
        if (!v->ui_ready) { ok = false; goto cleanup; }
        SDL_strlcpy(v->name, name, sizeof(v->name));
    }
    SDL_SetWindowAlwaysOnTop(v->window, app->settings.window.always_on_top);
    if (!app->session.window.visible || app->window_minimized || app->hover_hidden || app->settings.window.capture_only) {
        SDL_HideWindow(v->window); goto cleanup;
    }
    uint64_t now = SDL_GetTicks();
    float elapsed = v->frame ? SDL_min((now - v->frame) / 1000.0f, 0.1f) : 0.033f;
    v->frame = now;
    for (uint32_t i = 0; i < BONGO_CAT_LAN_PARAMETERS; ++i)
        if (snapshot->pose.mask & (1u << i))
            bongo_cat_model_runtime_set_parameter(v->model, bongo_cat_lan_parameters[i], snapshot->pose.values[i]);
    bongo_cat_overlay_set_mver_pointer(v->overlay, snapshot->pose.pointer_x, snapshot->pose.pointer_y,
        (snapshot->pose.buttons & 1u) != 0, (snapshot->pose.buttons & 2u) != 0,
        (snapshot->pose.buttons & 4u) != 0);
    bongo_cat_model_runtime_update(v->model, elapsed);
    int width = 0, height = 0;
    SDL_GetWindowSizeInPixels(v->window, &width, &height);
    int label = height * LAN_NAME_HEIGHT / LAN_HEIGHT;
    glViewport(0, 0, width, height); glDisable(GL_SCISSOR_TEST);
    glClearColor(0, 0, 0, 0); glClear(GL_COLOR_BUFFER_BIT);
    bongo_cat_model_runtime_resize(v->model, width, height - label);
    glViewport(0, 0, width, height - label);
    bongo_cat_overlay_draw_background(v->overlay, false);
    bongo_cat_model_runtime_draw(v->model);
    glViewport(0, 0, width, height - label);
    bongo_cat_overlay_draw_pointer_before_keys(v->overlay);
    bongo_cat_overlay_draw_keys(v->overlay, false);
    bongo_cat_overlay_draw_pointer_after_keys(v->overlay);
    glViewport(0, 0, width, height);
    ok = bongo_cat_lan_name_draw(&v->ui, name) && bongo_cat_ui_present(v->window);
    if (ok) SDL_ShowWindow(v->window);
cleanup:
    if (!ok) bongo_cat_lan_surface_destroy(slot);
    SDL_GL_MakeCurrent(previous, context);
    return ok;
}
bool bongo_cat_lan_surface_event(BongoCatLanSurface *v, const SDL_Event *event) {
    if (!v || SDL_GetWindowFromEvent(event) != v->window) return false;
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_LEFT) {
        v->dragging = true; v->drag_x = event->button.x; v->drag_y = event->button.y;
        SDL_CaptureMouse(true);
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        v->dragging = false; SDL_CaptureMouse(false);
    } else if (event->type == SDL_EVENT_MOUSE_MOTION && v->dragging) {
        int x = 0, y = 0;
        SDL_GetWindowPosition(v->window, &x, &y);
        SDL_SetWindowPosition(v->window, x + (int)(event->motion.x - v->drag_x),
            y + (int)(event->motion.y - v->drag_y));
    }
    return true;
}
