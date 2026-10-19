#include "ui_software.h"
#include "preferences_controls.h"
#include "plugin_ui_internal.h"
#include "preferences_widgets.h"
#include "ui_catime.h"
#include "ui_software_font.h"
#include "ui_font_atlas.h"
#include "ui_paint_cache.h"
#include <stdlib.h>
#include "test.h"
#include <math.h>
#include <string.h>
int bongo_cat_test_failures;
static unsigned slice_count;
static bool resize_pending;
static void draw(BongoCatUIBackend *ui, unsigned red, unsigned alpha) {
    SDL_SetRenderDrawColor(ui->software_renderer, 0, 0, 0, 255);
    SDL_RenderClear(ui->software_renderer);
    if (nk_begin(&ui->context, "test", nk_rect(0, 0, 128, 128), NK_WINDOW_NO_SCROLLBAR)) {
        nk_fill_triangle(nk_window_get_canvas(&ui->context),10,30,20,40,30,30,nk_rgba(20,30,40,255));
        nk_layout_row_dynamic(&ui->context, 24, 1);
        nk_label(&ui->context, "Text with spaces", NK_TEXT_LEFT);
        struct nk_image image = nk_image_id((int)red);
        nk_draw_image(nk_window_get_canvas(&ui->context), nk_rect(64, 64, 16, 16), &image, nk_rgba(255,255,255,255));
        image = nk_image_id((int)alpha);
        nk_draw_image(nk_window_get_canvas(&ui->context), nk_rect(84, 64, 16, 16), &image, nk_rgba(255,255,255,255));
    }
    nk_end(&ui->context);
    CHECK(bongo_cat_ui_render(ui));
    if (resize_pending) {
        CHECK(ui->software_pending);
        CHECK(SDL_SetWindowSize(ui->window,160,160));
    }
    for (unsigned i = 0; ui->software_pending && i < 4096; ++i) {
        CHECK(bongo_cat_ui_software_render(ui,128,128)); slice_count++;
    }
    CHECK(!ui->software_pending && bongo_cat_ui_frame_valid(ui));
}
static void color_picker_edges(BongoCatUIBackend *ui) {
    struct nk_context *ctx = &ui->context;
    const float positions[] = {12, 230};
    for (size_t i = 0; i < SDL_arraysize(positions); ++i) {
        BongoCatPrefColorState state = {0};
        state.open = true;
        uint32_t rgb = 0x4455cc;
        nk_input_begin(ctx); nk_input_end(ctx);
        if (nk_begin(ctx, "picker-test", nk_rect(0, 0, 420, 320), NK_WINDOW_NO_SCROLLBAR)) {
            struct nk_rect clip = ctx->current->layout->clip;
            nk_layout_space_begin(ctx, NK_STATIC, 280, 1);
            nk_layout_space_push(ctx, nk_rect(350, positions[i], 32, 32));
            CHECK(!bongo_cat_pref_color_picker(ctx, &state, &rgb));
            CHECK(state.open && ctx->current->popup.active);
            struct nk_rect popup = ctx->current->popup.win->bounds;
            CHECK(popup.x >= clip.x && popup.y >= clip.y);
            CHECK(popup.x + popup.w <= clip.x + clip.w);
            CHECK(popup.y + popup.h <= clip.y + clip.h);
            CHECK(popup.w == 280);
            nk_layout_space_end(ctx);
        }
        nk_end(ctx);
        unsigned gradients = 0;
        const struct nk_command *command;
        nk_foreach(command, ctx) if (command->type == NK_COMMAND_RECT_MULTI_COLOR) {
            const struct nk_command_rect_multi_color *gradient =
                (const struct nk_command_rect_multi_color *)command;
            CHECK(gradient->x >= 0 && gradient->x + gradient->w <= 420);
            CHECK(gradient->y >= 0 && gradient->y + gradient->h <= 320);
            gradients++;
        }
        CHECK(gradients == 7); /* Full SV square and all six hue strips. */
        nk_clear(ctx);
    }
}
static void secondary_palette(BongoCatUIBackend *ui) {
    struct nk_context *ctx = &ui->context;
    const unsigned colors[] = {0x008000, 0x800080};
    struct nk_color previous = {0};
    for (size_t i = 0; i < SDL_arraysize(colors); ++i) {
        bongo_cat_ui_palette_colors(0x1000000u, colors[i]);
        BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(ctx));
        if (i) CHECK(memcmp(&previous, &p.hover_pink, sizeof(previous)) != 0);
        previous = p.hover_pink;
        nk_input_begin(ctx); nk_input_end(ctx);
        if (nk_begin(ctx, "palette-test", nk_rect(0, 0, 420, 320), NK_WINDOW_NO_SCROLLBAR))
        {
            bongo_cat_pref_intro(ctx, "Secondary tone");
            BongoCatPluginUi plugin = {0};
            plugin.context = ctx;
            nk_layout_row_dynamic(ctx, 32, 1);
            (void)bongo_cat_preferences_plugin_canvas()->button(&plugin, "secondary", "Secondary", BONGO_UI_BUTTON_SECONDARY);
        }
        nk_end(ctx);
        bool found = false, button_found = false, label_found = false;
        const struct nk_command *command;
        nk_foreach(command, ctx) if (command->type == NK_COMMAND_RECT_FILLED) {
            const struct nk_command_rect_filled *rect = (const struct nk_command_rect_filled *)command;
            if (!memcmp(&rect->color, &p.hover_pink, sizeof(rect->color))) found = true;
            if (!memcmp(&rect->color, &p.pink, sizeof(rect->color))) button_found = true;
        }
        nk_foreach(command, ctx) if (command->type == NK_COMMAND_TEXT) {
            const struct nk_command_text *text = (const struct nk_command_text *)command;
            if (!memcmp(&text->foreground, &p.on_secondary, sizeof(text->foreground))) label_found = true;
        }
        CHECK(found && button_found && label_found);
        nk_clear(ctx);
    }
    bongo_cat_ui_palette_colors(0x2244aa, 0x008040);
    bongo_cat_ui_palette_text(0x101010);
    BongoCatUIPalette before = bongo_cat_ui_palette(false);
    bongo_cat_ui_palette_text(0xaa2020);
    BongoCatUIPalette after = bongo_cat_ui_palette(false);
    CHECK(memcmp(&before.text, &after.text, sizeof(before.text)) != 0);
    CHECK(!memcmp(&before.accent_hover, &after.accent_hover, sizeof(before.accent_hover)));
    CHECK(!memcmp(&before.accent_pressed, &after.accent_pressed, sizeof(before.accent_pressed)));
    CHECK(!memcmp(&before.pink_hover, &after.pink_hover, sizeof(before.pink_hover)));
    CHECK(!memcmp(&before.on_secondary, &after.on_secondary, sizeof(before.on_secondary)));
    bongo_cat_ui_palette_text(0x1000000u);
    bongo_cat_ui_palette_colors(0x1000000u, 0x1000000u);
}
static void plugin_controls(BongoCatUIBackend *ui) {
    struct nk_context *ctx=&ui->context;
    nk_input_begin(ctx);nk_input_end(ctx);
    if (nk_begin(ctx,"plugin-controls",nk_rect(0,0,420,600),NK_WINDOW_NO_SCROLLBAR)) {
        BongoCatPluginUi plugin={0};plugin.context=ctx;
        const BongoCatPluginPageUiHost *host=bongo_cat_preferences_plugin_canvas();
        float short_label=bongo_cat_preferences_plugin_text_height(&plugin,"Short",120,30);
        float long_label=bongo_cat_preferences_plugin_text_height(&plugin,
            "A translated plugin description that wraps across several lines",120,30);
        CHECK(long_label>short_label);
        host->widgets->label_wrapped(&plugin,
            "A translated plugin description that wraps across several lines",1);
        CHECK(ctx->current->layout->row.height>ctx->style.font->height);
        host->disabled(&plugin,1);host->disabled(&plugin,1);
        CHECK(ctx->current->widgets_disabled);
        host->disabled(&plugin,0);
        CHECK(ctx->current->widgets_disabled && (ctx->current->layout->flags & NK_WINDOW_ROM));
        host->disabled(&plugin,0);
        CHECK(!ctx->current->widgets_disabled && !(ctx->current->layout->flags & NK_WINDOW_ROM));
        host->disabled(&plugin,0); /* Unmatched legacy ends must not alter the current panel. */
        ctx->current->layout->flags|=NK_WINDOW_ROM;
        host->disabled(&plugin,1);host->disabled(&plugin,0);
        CHECK(ctx->current->layout->flags & NK_WINDOW_ROM);
        ctx->current->layout->flags &= ~(nk_flags)NK_WINDOW_ROM;
    }
    nk_end(ctx);nk_clear(ctx);
}
int main(void) {
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE);
    CHECK(SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *window = SDL_CreateWindow("test", 128, 128, SDL_WINDOW_HIDDEN);
    CHECK(window != NULL);
    if (!window) { SDL_Quit(); return 1; }
    SDL_SetBooleanProperty(SDL_GetWindowProperties(window), "BongoCat.SoftwareUI", true);
    BongoCatUIBackend ui;
    BongoCatError error = {0};
    bool initialized = bongo_cat_ui_init(&ui, window, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 1, 1, &error);
    CHECK(initialized);
    if (!initialized) { SDL_DestroyWindow(window); SDL_Quit(); return 1; }
    CHECK(ui.software_renderer && !ui.program && !SDL_GL_GetCurrentContext());
    color_picker_edges(&ui);
    secondary_palette(&ui);
    plugin_controls(&ui);
    CHECK(ui.caption_font->userdata.ptr != ui.hero_font->userdata.ptr);
    CHECK(ui.caption_font->height == 18 && ui.hero_font->height == 36);
    BongoCatUIPaintKey small={0}, large={0}, rejected={0};
    small.width=small.height=100; large.width=large.height=300;
    rejected.width=400; rejected.height=200;
    unsigned char *paint_pixels=calloc(300*300,4);
    CHECK(paint_pixels != NULL);
    if (paint_pixels) {
        bongo_cat_ui_paint_cache_begin_frame(&ui);
        BongoCatUIPaintTexture *first=bongo_cat_ui_paint_cache_get(&ui,&small);
        CHECK(first && bongo_cat_ui_paint_cache_upload(first,paint_pixels,false));
        BongoCatUIPaintTexture *second=bongo_cat_ui_paint_cache_get(&ui,&large);
        CHECK(second && bongo_cat_ui_paint_cache_upload(second,paint_pixels,false));
        CHECK(bongo_cat_ui_paint_cache_get(&ui,&rejected)==NULL);
        CHECK(bongo_cat_ui_paint_cache_ready(first) && bongo_cat_ui_paint_cache_ready(second));
        CHECK(bongo_cat_ui_paint_cache_usage(&ui,NULL)==400000);
        bongo_cat_ui_paint_cache_begin_frame(&ui);
        BongoCatUIPaintTexture *next=bongo_cat_ui_paint_cache_get(&ui,&rejected);
        CHECK(next && bongo_cat_ui_paint_cache_upload(next,paint_pixels,false));
        CHECK(bongo_cat_ui_paint_cache_usage(&ui,NULL)==320000);
        first=bongo_cat_ui_paint_cache_get(&ui,&small);
        CHECK(first && bongo_cat_ui_paint_cache_upload(first,paint_pixels,false));
        CHECK(bongo_cat_ui_paint_cache_usage(&ui,NULL)==360000);
        bongo_cat_ui_paint_cache_begin_frame(&ui);
        CHECK(bongo_cat_ui_paint_cache_get(&ui,&rejected)==next);
        bongo_cat_ui_paint_cache_trim_idle(&ui);
        CHECK(bongo_cat_ui_paint_cache_usage(&ui,NULL)==320000);
        CHECK(bongo_cat_ui_paint_cache_ready(next));
        bongo_cat_ui_paint_cache_begin_frame(&ui);
        bongo_cat_ui_paint_cache_trim_idle(&ui);
        CHECK(bongo_cat_ui_paint_cache_usage(&ui,NULL)==0);
        bongo_cat_ui_paint_cache_destroy(&ui); free(paint_pixels);
    }
    const unsigned char rgba[] = {255, 0, 0, 255}, mask[] = {128};
    unsigned red = bongo_cat_ui_texture_upload(&ui, rgba, 1, 1, false);
    unsigned alpha = bongo_cat_ui_texture_upload(&ui, mask, 1, 1, true);
    CHECK(red && alpha && red != alpha);
    draw(&ui, red, alpha);
    void *larger = realloc(ui.vertices, 1024u * 1024u);
    CHECK(larger != NULL);
    if (larger) { ui.vertices = larger; ui.vertex_capacity = 1024u * 1024u; }
    larger = realloc(ui.elements, 512u * 1024u);
    CHECK(larger != NULL);
    if (larger) { ui.elements = larger; ui.element_capacity = 512u * 1024u; }
    size_t old_vertices = ui.vertex_capacity, old_elements = ui.element_capacity;
    // Both command buffers have been cleared but can retain a large allocation.
    struct nk_allocator allocator = ui.commands.pool;
    nk_buffer_free(&ui.commands);
    nk_buffer_init(&ui.commands, &allocator, 2u * 1024u * 1024u);
    allocator = ui.context.memory.pool;
    nk_buffer_free(&ui.context.memory);
    nk_buffer_init(&ui.context.memory, &allocator, 2u * 1024u * 1024u);
    CHECK(!ui.commands.allocated && !ui.context.memory.allocated);
    bongo_cat_ui_trim_idle(&ui);
    CHECK(nk_buffer_total(&ui.commands) < 2u * 1024u * 1024u &&
        nk_buffer_total(&ui.context.memory) < 2u * 1024u * 1024u);
    CHECK(ui.vertex_capacity < old_vertices && ui.element_capacity < old_elements);
    CHECK(ui.vertex_capacity >= ui.last_vertex_bytes &&
        ui.element_capacity >= ui.last_element_bytes);
    draw(&ui, red, alpha); // Trimming must preserve the next full render.
    SDL_Surface *pixels = SDL_RenderReadPixels(ui.software_renderer, NULL);
    CHECK(pixels != NULL);
    if (pixels) {
        Uint8 r, g, b, a;
        CHECK(SDL_ReadSurfacePixel(pixels, 70, 70, &r, &g, &b, &a));
        CHECK(r > 200 && g < 20 && b < 20);
        CHECK(SDL_ReadSurfacePixel(pixels, 90, 70, &r, &g, &b, &a));
        CHECK(r > 100 && r < 160 && r == g && g == b);
        SDL_DestroySurface(pixels);
    }
    CHECK(bongo_cat_ui_font_atlas_reload(&ui, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 1));
    CHECK(ui.caption_font == &ui.font_handles[0] && ui.hero_font == &ui.font_handles[4]);
    draw(&ui, red, alpha); // Reload preserves existing texture IDs and live font handles.
    CHECK(bongo_cat_ui_font_atlas_reload(&ui, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 2));
    const struct nk_font *physical_font = ui.body_font->userdata.ptr;
    CHECK(physical_font && fabsf(physical_font->info.height - 40) < .01f);
    CHECK(ui.body_font->height == 20); // Logical layout remains stable at 200% DPI.
    CHECK(bongo_cat_ui_font_atlas_reload(&ui, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 1));

    SDL_SetBooleanProperty(SDL_GetWindowProperties(window), "BongoCat.AsyncSoftwareUI", true);
    SDL_SetNumberProperty(SDL_GetWindowProperties(window), "BongoCat.SoftwareSliceBudgetNS", 1);
    draw(&ui, red, alpha);
    CHECK(slice_count > 0);
    resize_pending = true;
    draw(&ui, red, alpha);
    pixels = SDL_RenderReadPixels(ui.software_renderer,NULL);
    CHECK(pixels && pixels->w == 160 && pixels->h == 160);
    if (pixels) {
        Uint8 r,g,b,a;
        CHECK(SDL_ReadSurfacePixel(pixels,88,88,&r,&g,&b,&a));
        CHECK(r > 200 && g < 20 && b < 20);
        SDL_DestroySurface(pixels);
    }
    resize_pending = false;
    CHECK(SDL_SetWindowSize(window,128,128));
    CHECK(ui.font_atlas_height > 0);
    const struct nk_font *font = ui.body_font->userdata.ptr;
    for (nk_rune i = 0; font && i < font->info.glyph_count; ++i)
        CHECK(font->glyphs[i].v0 >= 0 && font->glyphs[i].v1 <= 1);

    SDL_SetBooleanProperty(SDL_GetWindowProperties(window), "BongoCat.AsyncSoftwareUI", false);
    unsigned ids[512] = {0}; int count = 0;
    while (count < 512 && (ids[count] = bongo_cat_ui_texture_upload(&ui, rgba, 1, 1, false))) count++;
    CHECK(count > 0 && count < 512);
    CHECK(bongo_cat_ui_texture_upload(&ui, rgba, 1, 1, false) == 0);
    unsigned freed = ids[0];
    bongo_cat_ui_texture_delete(&ui, 1, &ids[0]);
    ids[0] = bongo_cat_ui_texture_upload(&ui, rgba, 1, 1, false);
    CHECK(ids[0] == freed);
    bongo_cat_ui_texture_delete(&ui, count, ids);
    bongo_cat_ui_texture_delete(&ui, 1, &red);
    bongo_cat_ui_texture_delete(&ui, 1, &alpha);
    // NPOT glyph UVs and fractional display scaling must select the same
    // texel. Reusing the scratch must preserve already rendered glyphs.
    unsigned char glyph[7*17]={0}; glyph[11*7+3]=255;
    BongoCatUIFontMask *mask_font=bongo_cat_ui_font_mask_create(glyph,7,17);
    SDL_Texture *scratch=SDL_CreateTexture(ui.software_renderer,SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,1,1);
    CHECK(mask_font && scratch);
    SDL_SetRenderDrawColor(ui.software_renderer,0,0,0,255);
    SDL_RenderClear(ui.software_renderer);
    SDL_SetRenderScale(ui.software_renderer,1.25f,1.25f);
    SDL_FRect texel={3.0f/7,11.0f/17,1.0f/7,1.0f/17};
    CHECK(bongo_cat_ui_font_mask_draw(ui.software_renderer,mask_font,&scratch,texel,
        (SDL_FRect){16,16,8,8},(SDL_FColor){1,0,0,1},1.25f,1.25f));
    CHECK(bongo_cat_ui_font_mask_draw(ui.software_renderer,mask_font,&scratch,texel,
        (SDL_FRect){32,16,8,8},(SDL_FColor){0,1,0,1},1.25f,1.25f));
    pixels=SDL_RenderReadPixels(ui.software_renderer,NULL);
    CHECK(pixels != NULL);
    if (pixels) {
        Uint8 r,g,b,a;
        CHECK(SDL_ReadSurfacePixel(pixels,25,25,&r,&g,&b,&a));
        CHECK(r > 220 && g < 20 && b < 20);
        CHECK(SDL_ReadSurfacePixel(pixels,45,25,&r,&g,&b,&a));
        CHECK(g > 220 && r < 20 && b < 20);
        SDL_DestroySurface(pixels);
    }
    CHECK(!bongo_cat_ui_font_mask_draw(ui.software_renderer,mask_font,&scratch,
        (SDL_FRect){2,0,1,1},(SDL_FRect){0,0,1,1},(SDL_FColor){1,1,1,1},1,1));
    SDL_SetRenderScale(ui.software_renderer,1,1);
    SDL_DestroyTexture(scratch); bongo_cat_ui_font_mask_free(mask_font);
    if (nk_begin(&ui.context, "invalid", nk_rect(0,0,128,128), NK_WINDOW_NO_SCROLLBAR)) {
        struct nk_image invalid = nk_image_id(999);
        nk_draw_image(nk_window_get_canvas(&ui.context), nk_rect(10,10,16,16), &invalid, nk_rgba(255,255,255,255));
    }
    nk_end(&ui.context);
    CHECK(!bongo_cat_ui_render(&ui));
    CHECK(!bongo_cat_ui_frame_valid(&ui));
    SDL_SetBooleanProperty(SDL_GetWindowProperties(window), "BongoCat.AsyncSoftwareUI", true);
    if (nk_begin(&ui.context,"pending-destroy",nk_rect(0,0,128,128),NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_dynamic(&ui.context,24,1);
        nk_label(&ui.context,"Pending frame",NK_TEXT_LEFT);
    }
    nk_end(&ui.context);
    CHECK(bongo_cat_ui_render(&ui) && ui.software_pending);
    void *pending_vertices = ui.vertices, *pending_elements = ui.elements;
    size_t pending_commands = nk_buffer_total(&ui.commands);
    void *pending_memory = nk_buffer_memory(&ui.context.memory);
    bongo_cat_ui_trim_idle(&ui);
    CHECK(ui.software_pending && ui.vertices == pending_vertices &&
        ui.elements == pending_elements && nk_buffer_total(&ui.commands) == pending_commands &&
        nk_buffer_memory(&ui.context.memory) == pending_memory);
    bongo_cat_ui_destroy(&ui);
    CHECK(SDL_GetPointerProperty(SDL_GetWindowProperties(window), "BongoCat.SoftwareUIRenderer", NULL) == NULL);
    SDL_DestroyWindow(window); SDL_Quit();
    return bongo_cat_test_failures ? 1 : 0;
}
