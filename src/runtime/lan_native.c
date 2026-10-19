#include "lan_native.h"
#include "lan_graphics.h"
#include "bongo_cat/overlay.h"
#include "bongo_cat/log.h"
#include "model_import.h"
#include "mver/mver_config.h"
#include "mver/mver_render.h"
#include "ui_present.h"
#include <string.h>
#ifdef _WIN32
#include "windows_layered.h"
#include "windows_hdr.h"
#endif
#define LAN_WIDTH 480
#define LAN_HEIGHT 320
#define LAN_NAME_HEIGHT 28
struct BongoCatLanNative {
    SDL_Window *window;
#ifdef _WIN32
    BongoCatPlatform presentation;
    BongoCatRhiPresentOps present_ops;
#endif
    BongoCatRhi rhi;
    BongoCatModelRuntime *model;
    BongoCatOverlay *overlay;
    BongoCatUIBackend ui;
    SDL_Texture *texture;
    unsigned char *pixels;
    uint32_t generation;
    uint64_t frame;
    bool ui_ready;
    char name[128];
};
SDL_Window *bongo_cat_lan_native_window(BongoCatLanNative *v) { return v ? v->window : NULL; }
void bongo_cat_lan_native_destroy(BongoCatLanNative **slot) {
    BongoCatLanNative *v = *slot;
    if (!v) return;
    bongo_cat_rhi_wait_idle(&v->rhi);
    bongo_cat_model_runtime_destroy(v->model);
    bongo_cat_overlay_destroy(v->overlay);
    SDL_DestroyTexture(v->texture);
    if (v->ui_ready) bongo_cat_ui_destroy(&v->ui);
    bongo_cat_rhi_destroy(&v->rhi);
#ifdef _WIN32
    bongo_cat_windows_layered_destroy(&v->presentation);
#endif
    SDL_DestroyWindow(v->window); SDL_free(v->pixels); SDL_free(v); *slot = NULL;
}
static bool draw_model(void *context) {
    BongoCatLanNative *v = context;
    BongoCatRhiDeviceInfo info;
    if (!bongo_cat_rhi_get_device_info(&v->rhi, &info)) return false;
    bongo_cat_model_runtime_set_rhi_info(v->model, &info);
    return !bongo_cat_model_runtime_rendering(v->model) || bongo_cat_model_runtime_draw_checked(v->model);
}
#ifdef _WIN32
static bool read_composite(int width, int height, void *pixels, void *context) {
    BongoCatLanNative *v = context;
    SDL_Surface *surface = SDL_RenderReadPixels(v->ui.software_renderer, NULL);
    bool ok = surface && surface->w == width && surface->h == height &&
        SDL_FlipSurface(surface, SDL_FLIP_VERTICAL) && SDL_ConvertPixels(width, height,
            surface->format, surface->pixels, surface->pitch, SDL_PIXELFORMAT_BGRA32, pixels, width * 4);
    SDL_DestroySurface(surface); return ok;
}
static bool swap_composite(SDL_Window *window, void *context) {
    (void)window; (void)context; return true;
}
static const char *composite_name(void *context) { (void)context; return "Vulkan friend composite"; }
#endif
static bool create(BongoCatApp *app, BongoCatLanNative **slot,
    const BongoCatLanSnapshot *snapshot, const char *name) {
    BongoCatLanNative *v = SDL_calloc(1, sizeof(*v));
    if (!v) return false;
    *slot = v; v->generation = snapshot->generation;
    v->window = SDL_CreateWindow(name, LAN_WIDTH, LAN_HEIGHT,
        SDL_WINDOW_TRANSPARENT | SDL_WINDOW_BORDERLESS | SDL_WINDOW_UTILITY | SDL_WINDOW_HIDDEN);
    if (!v->window || !SDL_SetBooleanProperty(SDL_GetWindowProperties(v->window),
        "BongoCat.SoftwareUI", true)) return false;
    BongoCatError error = {0};
    if (!bongo_cat_rhi_create_offscreen(&app->rhi, v->window, LAN_WIDTH,
        LAN_HEIGHT - LAN_NAME_HEIGHT, &v->rhi, &error)) return false;
    v->ui_ready = bongo_cat_lan_name_init(app, &v->ui, v->window, name);
    if (!v->ui_ready) return false;
#ifdef _WIN32
    v->presentation.window = v->window; v->presentation.window_opacity = 1.0f;
    v->presentation.presenter = bongo_cat_windows_layered_create(true);
    if (!v->presentation.presenter) return false;
    v->present_ops.read_bgra = read_composite; v->present_ops.swap = swap_composite;
    v->present_ops.name = composite_name; v->present_ops.requires_layered = true; v->present_ops.user = v;
    v->presentation.present_ops = &v->present_ops;
#endif
    v->pixels = SDL_malloc(LAN_WIDTH * (LAN_HEIGHT - LAN_NAME_HEIGHT) * 4u);
    v->texture = SDL_CreateTexture(v->ui.software_renderer, SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING, LAN_WIDTH, LAN_HEIGHT - LAN_NAME_HEIGHT);
    if (!v->pixels || !v->texture || !SDL_SetTextureBlendMode(v->texture, SDL_BLENDMODE_NONE)) return false;
    v->model = bongo_cat_model_runtime_create(app->asset_root, &error);
    v->overlay = bongo_cat_overlay_create(&error);
    if (!v->model || !v->overlay) return false;
    BongoCatRhiDeviceInfo info;
    if (!bongo_cat_rhi_get_device_info(&v->rhi, &info)) return false;
    bongo_cat_model_runtime_set_rhi_info(v->model, &info);
    BongoCatModelRuntimeRenderOptions options = {0};
    options.projection_scale = 1.0f;
    char config[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_import_render_options(snapshot->model.adapter, &options) &&
        bongo_cat_mver_config_find(snapshot->model.directory, config, sizeof(config)))
        (void)bongo_cat_mver_render_read(config, &options);
    if (bongo_cat_model_runtime_load(v->model, snapshot->model.directory,
        snapshot->model.setting, false, &options, NULL, NULL, &error) != BONGO_CAT_OK) {
        SDL_LogWarn(BONGO_CAT_LOG_LIFECYCLE, "[lan] Vulkan model load failed: %s", error.message);
        return false;
    }
    (void)bongo_cat_overlay_load(v->overlay, snapshot->model.adapter, false, bongo_cat_model_runtime_rendering(v->model), &options, &error);
    v->rhi.present.draw_frame = draw_model; v->rhi.present.hook_user = v;
    SDL_strlcpy(v->name, name, sizeof(v->name));
    bongo_cat_lan_position(app,v->window,snapshot->peer_id);
    return true;
}
static bool paint(BongoCatLanNative *v) {
    const int height = LAN_HEIGHT - LAN_NAME_HEIGHT;
    bongo_cat_model_runtime_resize(v->model, LAN_WIDTH, height);
    bongo_cat_rhi_prepare_frame(&v->rhi, LAN_WIDTH, height);
    bongo_cat_rhi_clear(&v->rhi, 0, 0, 0, 0);
    int x = 0, y = 0, cw = LAN_WIDTH, ch = height;
    (void)bongo_cat_model_runtime_overlay_viewport(v->model, &x, &y, &cw, &ch);
    if (!bongo_cat_overlay_prepare_native_background(v->overlay, &v->rhi,
        LAN_WIDTH, height, x, y, cw, ch, false, false, false, 0) ||
        !bongo_cat_rhi_render_frame(&v->rhi)) return false;
    bool copied = v->rhi.present.read_rgba(0, 0, LAN_WIDTH, height, true,
        v->pixels, v->rhi.present.user);
    bool retired = v->rhi.present.swap(v->window, v->rhi.present.user);
    if (!copied || !retired || !SDL_UpdateTexture(v->texture, NULL, v->pixels, LAN_WIDTH * 4)) return false;
    SDL_Renderer *renderer = v->ui.software_renderer;
    if (!SDL_SetRenderScale(renderer, 1, 1) || !SDL_SetRenderClipRect(renderer, NULL) ||
        !SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0) || !SDL_RenderClear(renderer)) return false;
    SDL_FRect target = {0, LAN_NAME_HEIGHT, LAN_WIDTH, (float)height};
    if (!SDL_RenderTextureRotated(renderer, v->texture, NULL, &target, 0, NULL, SDL_FLIP_VERTICAL) ||
        !bongo_cat_lan_name_draw(&v->ui, v->name)) return false;
#ifdef _WIN32
    SDL_ShowWindow(v->window);
    return bongo_cat_platform_present(&v->presentation, LAN_WIDTH, LAN_HEIGHT);
#else
    return bongo_cat_ui_present(v->window);
#endif
}
bool bongo_cat_lan_native_update(BongoCatApp *app, BongoCatLanNative **slot,
    const BongoCatLanSnapshot *snapshot, const char *name) {
    if (*slot && (*slot)->generation != snapshot->generation) bongo_cat_lan_native_destroy(slot);
    if (!*slot && !create(app, slot, snapshot, name)) return false;
    BongoCatLanNative *v = *slot;
    if (strcmp(v->name, name)) {
        SDL_DestroyTexture(v->texture); v->texture = NULL;
        bongo_cat_ui_destroy(&v->ui); v->ui_ready = false;
        v->ui_ready = bongo_cat_lan_name_init(app, &v->ui, v->window, name);
        if (!v->ui_ready) return false;
        v->texture = SDL_CreateTexture(v->ui.software_renderer, SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STREAMING, LAN_WIDTH, LAN_HEIGHT - LAN_NAME_HEIGHT);
        if (!v->texture || !SDL_SetTextureBlendMode(v->texture, SDL_BLENDMODE_NONE)) return false;
        SDL_strlcpy(v->name, name, sizeof(v->name));
    }
    SDL_SetWindowAlwaysOnTop(v->window, app->settings.window.always_on_top);
    if (!app->session.window.visible || app->window_minimized || app->hover_hidden || app->settings.window.capture_only) {
        SDL_HideWindow(v->window); return true;
    }
    uint64_t now = SDL_GetTicks();
    float elapsed = v->frame ? SDL_min((now - v->frame) / 1000.0f, 0.1f) : 0.033f;
    v->frame = now;
    for (uint32_t i = 0; i < BONGO_CAT_LAN_PARAMETERS; ++i)
        if (snapshot->pose.mask & (1u << i)) bongo_cat_model_runtime_set_parameter(v->model,
            bongo_cat_lan_parameters[i], snapshot->pose.values[i]);
    bongo_cat_overlay_set_mver_pointer(v->overlay, snapshot->pose.pointer_x, snapshot->pose.pointer_y,
        (snapshot->pose.buttons & 1u) != 0, (snapshot->pose.buttons & 2u) != 0, (snapshot->pose.buttons & 4u) != 0);
    bongo_cat_model_runtime_update(v->model, elapsed);
    if (!paint(v)) return false;
    SDL_ShowWindow(v->window); return true;
}
