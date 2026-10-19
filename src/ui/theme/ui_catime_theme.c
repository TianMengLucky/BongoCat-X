#include "ui_catime.h"
#include "ui_backend.h"
#include "ui_paint.h"
#include <math.h>

/* The app owns a single settings palette shared by its popup contexts. */
#define DEFAULT_COLOR 0x1000000u
static unsigned background_color = DEFAULT_COLOR;
static unsigned text_color = DEFAULT_COLOR;
static unsigned primary_color = DEFAULT_COLOR;
static unsigned secondary_color = DEFAULT_COLOR;
static BongoCatUIPalette cached_palette[2];
static bool palette_valid[2];

void bongo_cat_ui_palette_background(unsigned color) {
    if (background_color == color) return;
    background_color = color;
    palette_valid[0] = palette_valid[1] = false;
}

void bongo_cat_ui_palette_text(unsigned color) {
    if (text_color == color) return;
    text_color = color;
    palette_valid[0] = palette_valid[1] = false;
}

void bongo_cat_ui_palette_colors(unsigned primary, unsigned secondary) {
    primary_color = primary;
    secondary_color = secondary;
    palette_valid[0] = palette_valid[1] = false;
}

#ifdef _WIN32
#include <windows.h>
#endif

static struct nk_color rgb(int value) {
    return nk_rgba((value >> 16) & 255, (value >> 8) & 255, value & 255, 255);
}

static float luminance(struct nk_color color) {
    float channels[] = {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f};
    for (unsigned i = 0; i < 3; ++i)
        channels[i] = channels[i] <= .04045f ? channels[i] / 12.92f :
            powf((channels[i] + .055f) / 1.055f, 2.4f);
    return .2126f * channels[0] + .7152f * channels[1] + .0722f * channels[2];
}

static struct nk_color readable_color(unsigned color, struct nk_color surface) {
    struct nk_color result = rgb((int)color);
    struct nk_color target = luminance(surface) < .179f ?
        nk_rgb(255, 255, 255) : nk_rgb(0, 0, 0);
    for (unsigned i = 0; i < 32; ++i) {
        float a = luminance(result), b = luminance(surface);
        if ((NK_MAX(a, b) + .05f) / (NK_MIN(a, b) + .05f) >= 4.5f) break;
        result = bongo_cat_ui_color_mix(result, target, .12f);
    }
    return result;
}

static struct nk_color button_hover(struct nk_color color, float amount) {
    struct nk_color target = luminance(color) > .179f ?
        nk_rgb(255, 255, 255) : nk_rgb(0, 0, 0);
    return bongo_cat_ui_color_mix(color, target, amount);
}

#ifdef _WIN32
static bool high_contrast(BongoCatUIPalette *value) {
    HIGHCONTRASTW contrast = {0}; contrast.cbSize = sizeof(contrast);
    if (!SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast),
        &contrast, 0) || !(contrast.dwFlags & HCF_HIGHCONTRASTON)) return false;
    COLORREF window = GetSysColor(COLOR_WINDOW);
    COLORREF text = GetSysColor(COLOR_WINDOWTEXT);
    COLORREF accent = GetSysColor(COLOR_HIGHLIGHT);
    #define NK_SYS(c) nk_rgb(GetRValue(c), GetGValue(c), GetBValue(c))
    value->background = value->surface = value->surface_glass =
        value->field = NK_SYS(window);
    value->border = value->border_subtle = value->text = value->muted =
        NK_SYS(text);
    value->link = value->link_hover = NK_SYS(text);
    value->on_secondary = value->on_accent = value->on_selection = NK_SYS(GetSysColor(COLOR_HIGHLIGHTTEXT));
    value->accent = value->accent_hover = value->accent_pressed = NK_SYS(accent);
    value->pink = value->pink_hover = NK_SYS(accent);
    value->hover = value->selection = NK_SYS(accent);
    value->hover_pink = NK_SYS(accent);
    value->danger = NK_SYS(GetSysColor(COLOR_HIGHLIGHTTEXT));
    value->danger_background = NK_SYS(accent);
    value->effects = false;
    #undef NK_SYS
    return true;
}
#endif

BongoCatUIPalette bongo_cat_ui_palette(bool dark) {
    BongoCatUIPalette value;
#ifdef _WIN32
    if (high_contrast(&value)) return value;
#endif
    const unsigned index = dark ? 1 : 0;
    if (palette_valid[index]) return cached_palette[index];
    bool custom_background = background_color <= 0xffffffu;
    if (custom_background) dark = luminance(rgb((int)background_color)) < .179f;
    value.background = rgb(dark ? 0x131722 : 0xF2F4F8);
    value.surface = rgb(dark ? 0x1C2230 : 0xFFFFFF);
    value.surface_glass = rgb(dark ? 0x161C28 : 0xF8F9FC);
    value.field = rgb(dark ? 0x252D3E : 0xEDF0F6);
    value.border = rgb(dark ? 0x52617A : 0xC5CDDC);
    value.border_subtle = rgb(dark ? 0x37435A : 0xDDE2EB);
    value.text = rgb(dark ? 0xEEF1FA : 0x202A40);
    value.muted = rgb(dark ? 0xA7B3CA : 0x596780);
    value.accent = rgb(dark ? 0xA0B5FF : 0x415DC2);
    value.on_accent = rgb(dark ? 0x131722 : 0xFFFFFF);
    value.accent_hover = rgb(dark ? 0xB6C6FF : 0x3C55B4);
    value.accent_pressed = rgb(dark ? 0x8FA5EF : 0x33499D);
    value.pink = rgb(dark ? 0xE8A5C1 : 0xA83D68);
    value.pink_hover = rgb(dark ? 0xF0B9CF : 0x913253);
    value.hover = rgb(dark ? 0x2A3348 : 0xE9EDF8);
    value.hover_pink = rgb(dark ? 0x3A2A39 : 0xF9EDF2);
    value.selection = rgb(dark ? 0x303C59 : 0xE1E7FA);
    value.on_selection = value.accent;
    value.danger = rgb(dark ? 0xFFABAB : 0xBD3049);
    value.danger_background = rgb(dark ? 0x472B35 : 0xFCECEF);
    if (custom_background) {
        value.surface_glass = rgb((int)background_color);
        struct nk_color edge = rgb(dark ? 0xFFFFFF : 0x000000);
        value.background = bongo_cat_ui_color_mix(value.surface_glass, edge, .04f);
        value.surface = bongo_cat_ui_color_mix(value.surface_glass, edge, .06f);
        value.field = bongo_cat_ui_color_mix(value.surface_glass, edge, .10f);
        value.border = bongo_cat_ui_color_mix(value.surface_glass, edge, .32f);
        value.border_subtle = bongo_cat_ui_color_mix(value.surface_glass, edge, .18f);
    }
    if (text_color <= 0xffffffu) value.text = rgb((int)text_color);
    if (text_color <= 0xffffffu || custom_background)
        value.muted = bongo_cat_ui_color_mix(value.text, value.surface_glass, .20f);
    if (primary_color <= 0xffffffu || custom_background) {
        unsigned accent = primary_color <= 0xffffffu ? primary_color :
            ((unsigned)value.accent.r << 16) | ((unsigned)value.accent.g << 8) | value.accent.b;
        value.accent = readable_color(accent, value.surface);
        value.accent_hover = button_hover(value.accent, .12f);
        value.accent_pressed = button_hover(value.accent, .24f);
        value.on_accent = luminance(value.accent) > .179f ?
            nk_rgb(0, 0, 0) : nk_rgb(255, 255, 255);
        value.selection = bongo_cat_ui_color_mix(value.surface, value.accent, .12f);
        value.hover = bongo_cat_ui_color_mix(value.surface, value.accent, .07f);
        value.on_selection = readable_color(accent, value.selection);
    }
    if (secondary_color <= 0xffffffu || custom_background) {
        unsigned secondary = secondary_color <= 0xffffffu ? secondary_color :
            ((unsigned)value.pink.r << 16) | ((unsigned)value.pink.g << 8) | value.pink.b;
        value.pink = readable_color(secondary, value.surface);
        value.pink_hover = button_hover(value.pink, .12f);
        value.hover_pink = bongo_cat_ui_color_mix(value.surface, value.pink, .08f);
    }
    value.on_secondary = luminance(value.pink) > .179f ?
        nk_rgb(0, 0, 0) : nk_rgb(255, 255, 255);
    /* Links retain their blue affordance independently of the editable accent. */
    value.link = readable_color(dark ? 0x93C5FDu : 0x1D4ED8u, value.surface);
    value.link_hover = readable_color(dark ? 0xBFDBFEu : 0x1E40AFu, value.surface);
    /* Flat surfaces avoid full-window glow and animated shadow textures. */
    value.effects = false;
    cached_palette[index] = value;
    palette_valid[index] = true;
    return value;
}

bool bongo_cat_ui_dark(const struct nk_context *context) {
    const BongoCatUIBackend *ui = bongo_cat_ui_backend_for_context(context);
    return ui ? ui->dark_theme :
        (context && context->style.window.background.r < 128);
}

static void geometry(struct nk_style *style) {
    style->window.padding = nk_vec2(18, 14);
    style->window.group_padding = nk_vec2(18, 12);
    style->window.spacing = nk_vec2(10, 10);
    style->window.scrollbar_size = nk_vec2(10, 10);
    style->window.border = 0;
    style->window.group_border = 0;
    style->window.rounding = 12;
    style->button.border = 1;
    style->button.rounding = 10;
    style->button.padding = nk_vec2(12, 3);
    style->button.text_alignment = NK_TEXT_CENTERED;
    style->edit.border = 1;
    style->edit.rounding = 10;
    style->edit.padding = nk_vec2(12, 8);
    style->combo.border = 1;
    style->combo.rounding = 10;
    style->combo.content_padding = nk_vec2(12, 8);
    style->combo.button.rounding = 8;
    style->selectable.rounding = 9;
    style->scrollv.rounding = 3;
    style->scrollv.rounding_cursor = 3;
    style->scrollv.padding = nk_vec2(2, 2);
    style->scrollh = style->scrollv;
}

void bongo_cat_ui_apply_theme(struct nk_context *context, bool dark) {
    struct nk_color table[NK_COLOR_COUNT];
    BongoCatUIPalette p = bongo_cat_ui_palette(dark);
    table[NK_COLOR_TEXT] = p.text;
    table[NK_COLOR_WINDOW] = p.background;
    table[NK_COLOR_HEADER] = p.surface;
    table[NK_COLOR_BORDER] = p.border_subtle;
    table[NK_COLOR_BUTTON] = p.accent;
    table[NK_COLOR_BUTTON_HOVER] = p.accent_hover;
    table[NK_COLOR_BUTTON_ACTIVE] = p.accent_pressed;
    table[NK_COLOR_TOGGLE] = p.field;
    table[NK_COLOR_TOGGLE_HOVER] = p.border_subtle;
    table[NK_COLOR_TOGGLE_CURSOR] = p.accent;
    table[NK_COLOR_SELECT] = p.field;
    table[NK_COLOR_SELECT_ACTIVE] = p.selection;
    table[NK_COLOR_SLIDER] = p.field;
    table[NK_COLOR_SLIDER_CURSOR] = p.accent;
    table[NK_COLOR_SLIDER_CURSOR_HOVER] = p.accent_hover;
    table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = p.accent_pressed;
    table[NK_COLOR_PROPERTY] = p.field;
    table[NK_COLOR_EDIT] = p.field;
    table[NK_COLOR_EDIT_CURSOR] = p.text;
    table[NK_COLOR_COMBO] = p.field;
    table[NK_COLOR_CHART] = p.surface;
    table[NK_COLOR_CHART_COLOR] = p.accent;
    table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = p.accent_hover;
    table[NK_COLOR_SCROLLBAR] = p.background;
    table[NK_COLOR_SCROLLBAR_CURSOR] = p.border_subtle;
    table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = p.accent;
    table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = p.accent;
    table[NK_COLOR_TAB_HEADER] = p.surface;
    table[NK_COLOR_KNOB] = p.field;
    table[NK_COLOR_KNOB_CURSOR] = p.accent;
    table[NK_COLOR_KNOB_CURSOR_HOVER] = p.accent_hover;
    table[NK_COLOR_KNOB_CURSOR_ACTIVE] = p.accent_pressed;
    nk_style_from_table(context, table);
    geometry(&context->style);
    context->style.button.text_normal = p.on_accent;
    context->style.button.text_hover = p.on_accent;
    context->style.button.text_active = p.on_accent;
    context->style.window.fixed_background = nk_style_item_color(p.background);
    context->style.window.group_border_color = p.border_subtle;
    context->style.text.color = p.text;
    BongoCatUIBackend *ui = bongo_cat_ui_backend_for_context(context);
    if (ui) ui->dark_theme = dark;
}
