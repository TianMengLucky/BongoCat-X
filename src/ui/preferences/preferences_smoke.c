#include "preferences_state.h"
#include "ui_paint_cache.h"
#include "bongo_cat/file.h"
#include "bongo_cat/path.h"

#include <SDL3/SDL_opengl.h>
#include <stdint.h>
#include <stdio.h>

static void send_key(BongoCatPreferences *value, Uint32 type, bool down) {
    SDL_Event event;
    SDL_zero(event);
    event.type = type;
    event.key.windowID = SDL_GetWindowID(value->window);
    event.key.down = down;
    event.key.scancode = SDL_SCANCODE_B;
    event.key.key = SDLK_B;
    event.key.mod = SDL_KMOD_CTRL | SDL_KMOD_SHIFT;
    bongo_cat_preferences_event(value, &event);
}

void bongo_cat_preferences_shortcut_smoke(BongoCatPreferences *value) {
    if (!value || !value->window || !value->app->smoke_preference_shortcut ||
        !value->shortcut_recording ||
        value->ui.context.input.mouse.buttons[NK_BUTTON_LEFT].down) return;
    value->app->smoke_preference_shortcut = false;
    send_key(value, SDL_EVENT_KEY_DOWN, true);
    send_key(value, SDL_EVENT_KEY_UP, false);
}

static float font_height(const struct nk_user_font *font) {
    return font ? font->height : 0.0f;
}

static void write_window_handle(BongoCatPreferences *value) {
    if (!value->app->smoke_preferences) return;
    uintptr_t native = 0;
#ifdef _WIN32
    native = (uintptr_t)SDL_GetPointerProperty(
        SDL_GetWindowProperties(value->window),
        SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
#endif
    char path[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_path_join(path, sizeof(path), value->app->state_root,
        "preferences-window.txt")) return;
    FILE *file = bongo_cat_file_open(path, "wb");
    if (!file) return;
    fprintf(file, "handle=%llu window_id=%u page=%d recording=%d shortcut_smoke=%d\n",
        (unsigned long long)native, (unsigned)SDL_GetWindowID(value->window),
        value->page, value->shortcut_recording,
        value->app->smoke_preference_shortcut);
    fclose(file);
}

void bongo_cat_preferences_record_frame(BongoCatPreferences *value) {
    if (!value->app->smoke_frame_series) return;
    char path[BONGO_CAT_PATH_CAP];
    bongo_cat_path_join(path, sizeof(path), value->app->state_root,
        "preferences-frames.csv");
    FILE *file = bongo_cat_file_open(path, "ab");
    if (!file) return;
    fprintf(file, "%llu\n", (unsigned long long)SDL_GetTicksNS());
    fclose(file);
}

void bongo_cat_preferences_smoke_frame(BongoCatPreferences *value) {
    if (!value) return;
    write_window_handle(value);
    if (!value->app->smoke) return;
    bool valid = bongo_cat_ui_frame_valid(&value->ui);
    bool assets_valid = value->logo_texture && value->icon_texture;
    if (!value->frame_checked) {
        value->frame_checked = true;
        if (!valid || !assets_valid) value->app->exit_code = 1;
    }
    int window_width = 0, window_height = 0;
    int pixel_width = 0, pixel_height = 0;
    float logical_width = 0.0f, logical_height = 0.0f;
    SDL_GetWindowSize(value->window, &window_width, &window_height);
    SDL_GetWindowSizeInPixels(value->window, &pixel_width, &pixel_height);
    bongo_cat_ui_logical_size(&value->ui, &logical_width, &logical_height);
    unsigned char corners[4][4] = {{0}};
    if (pixel_width > 0 && pixel_height > 0) {
        const int points[4][2] = {{0, 0}, {pixel_width - 1, 0},
            {0, pixel_height - 1}, {pixel_width - 1, pixel_height - 1}};
        for (int i = 0; i < 4; ++i) glReadPixels(points[i][0], points[i][1],
            1, 1, GL_RGBA, GL_UNSIGNED_BYTE, corners[i]);
    }
    unsigned corner_alpha = 0;
    for (int i = 0; i < 4; ++i)
        if (corners[i][3] > corner_alpha) corner_alpha = corners[i][3];
    char path[BONGO_CAT_PATH_CAP];
    bongo_cat_path_join(path, sizeof(path), value->app->state_root,
        "ui-frame.txt");
    FILE *file = bongo_cat_file_open(path, "wb");
    if (!file) return;
    size_t paint_count = 0;
    size_t paint_bytes = bongo_cat_ui_paint_cache_usage(&value->ui,
        &paint_count);
    fprintf(file, "valid=%d page=%d convert=%d vertices=%zu elements=%zu "
        "commands=%u draw_elements=%u gl_error=%u alpha_vertices=%u "
        "max_alpha=%u font_path=%d font_file=%d custom_font=%d font_probe=%d "
        "layout_scale=%.3f raster_scale=%.3f window=%dx%d pixels=%dx%d "
        "logical=%.2fx%.2f atlas=%dx%d transparent=%d corner_alpha=%u "
        "resize_cached=%u resize_failures=%u resize_layout=%u "
        "fonts=%.1f,%.1f,%.1f,%.1f,%.1f "
        "paint_textures=%zu paint_bytes=%zu "
        "assets=%u,%u,%u valid_assets=%d\n",
        valid, value->page, value->ui.last_convert_result,
        value->ui.last_vertex_bytes, value->ui.last_element_bytes,
        value->ui.last_draw_commands, value->ui.last_draw_elements,
        (unsigned)value->ui.last_gl_error, value->ui.nonzero_alpha_vertices,
        value->ui.max_alpha, value->ui.font_path_found,
        value->ui.font_file_loaded, value->ui.custom_font_loaded,
        value->ui.font_probe_loaded, value->ui.layout_scale,
        value->ui.raster_scale, window_width, window_height,
        pixel_width, pixel_height, logical_width, logical_height,
        value->ui.font_atlas_width, value->ui.font_atlas_height,
        value->transparent_window, corner_alpha,
        value->ui.resize_cache_presentations, value->ui.resize_cache_failures,
        value->live_resize_layout_frames,
        font_height(value->ui.caption_font), font_height(value->ui.body_font),
        font_height(value->ui.label_font), font_height(value->ui.heading_font),
        font_height(value->ui.hero_font), paint_count, paint_bytes,
        value->logo_texture, value->icon_texture,
        value->icon_texture_hidpi, assets_valid);
    fclose(file);
}
