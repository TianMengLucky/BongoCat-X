#ifndef BONGO_CAT_PLUGIN_UI_PAGE_H
#define BONGO_CAT_PLUGIN_UI_PAGE_H

#include "bongo_cat/plugin_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Optional ..._page_ui_query_v1 extension. The card ABI stays unchanged.
   All callbacks run on the preferences thread. Tables, strings and context
   are borrowed for this draw only; plugins own create/destroy state. */
#define BONGO_CAT_PLUGIN_PAGE_UI_ABI 1u

typedef struct BongoCatPluginUiRect { float x, y, w, h; } BongoCatPluginUiRect;
enum BongoCatPluginUiColor {
    BONGO_UI_TEXT, BONGO_UI_MUTED, BONGO_UI_SURFACE, BONGO_UI_FIELD,
    BONGO_UI_HOVER, BONGO_UI_SELECTION, BONGO_UI_BORDER, BONGO_UI_BORDER_SUBTLE,
    BONGO_UI_ACCENT, BONGO_UI_ACCENT_HOVER, BONGO_UI_DANGER,
    BONGO_UI_DANGER_BACKGROUND, BONGO_UI_LINK, BONGO_UI_LINK_HOVER
};
enum BongoCatPluginUiFont { BONGO_UI_BODY, BONGO_UI_CAPTION, BONGO_UI_LABEL, BONGO_UI_HEADING };
enum BongoCatPluginUiInput { BONGO_UI_HOVERED, BONGO_UI_CLICKED, BONGO_UI_HELD, BONGO_UI_POINTER };
enum BongoCatPluginUiButton { BONGO_UI_BUTTON, BONGO_UI_BUTTON_DANGER, BONGO_UI_BUTTON_LINK, BONGO_UI_BUTTON_SUCCESS };

typedef struct BongoCatPluginPageUiHost {
    uint32_t abi_version, struct_size;
    const BongoCatPluginUiHost *widgets;
    void (*section)(BongoCatPluginUi *, const char *);
    void (*status)(BongoCatPluginUi *, const char *, const char *, const char *);
    uint32_t (*segments)(BongoCatPluginUi *, const char *, const char *const *, uint32_t, uint32_t);
    uint32_t (*edit)(BongoCatPluginUi *, const char *, char *, uint32_t, uint32_t);
    void (*number)(BongoCatPluginUi *, const char *, const char *, double *, double, double);
    void (*disabled)(BongoCatPluginUi *, uint32_t);
    /* Reserve a widget in the current row. 0=clipped, 1=disabled, 2=interactive. */
    uint32_t (*region)(BongoCatPluginUi *, BongoCatPluginUiRect *);
    void (*fill_rect)(BongoCatPluginUi *, BongoCatPluginUiRect, float, uint32_t);
    void (*stroke_rect)(BongoCatPluginUi *, BongoCatPluginUiRect, float, float, uint32_t);
    void (*circle)(BongoCatPluginUi *, BongoCatPluginUiRect, uint32_t);
    void (*line)(BongoCatPluginUi *, BongoCatPluginUiRect, float, uint32_t);
    void (*draw_text)(BongoCatPluginUi *, BongoCatPluginUiRect, const char *, uint32_t, uint32_t, uint32_t);
    float (*measure)(BongoCatPluginUi *, const char *, uint32_t);
    uint32_t (*input)(BongoCatPluginUi *, BongoCatPluginUiRect, uint32_t);
    void (*pointer)(BongoCatPluginUi *, float *, float *);
    void (*tooltip)(BongoCatPluginUi *, const char *);
    uint32_t (*color)(BongoCatPluginUi *, uint32_t);
    float (*width)(BongoCatPluginUi *);
    float (*spacing)(BongoCatPluginUi *);
    uint32_t (*button)(BongoCatPluginUi *, const char *, const char *, uint32_t);
    uint32_t (*tree_begin)(BongoCatPluginUi *, const char *, const char *, uint32_t *);
    void (*tree_end)(BongoCatPluginUi *);
    void (*progress)(BongoCatPluginUi *, float);
    /* Paths are resource names, never graphics handles. The host owns the
       cache and fits the image into bounds. Returns 0 when unavailable. */
    uint32_t (*image)(BongoCatPluginUi *, const char *, BongoCatPluginUiRect);
    void (*images_begin)(BongoCatPluginUi *);
    void (*images_end)(BongoCatPluginUi *);
    void (*glyphs)(BongoCatPluginUi *, const char *const *, uint32_t, uint64_t *);
    void (*notice)(BongoCatPluginUi *, const char *, uint32_t);
    void (*invalidate)(BongoCatPluginUi *);
    void (*unfocus)(BongoCatPluginUi *);
} BongoCatPluginPageUiHost;

/* Data operations copy existing C ABI records into caller-owned buffers.
   The host checks the exact record size before touching a buffer. Actions
   never disable/unload a library. No worker-owned instance crosses the UI ABI. */
enum BongoCatPluginUiRead {
    BONGO_UI_READ_STATUS, BONGO_UI_READ_SNAPSHOT, BONGO_UI_READ_ENTRY,
    BONGO_UI_READ_KEY, BONGO_UI_READ_USAGE, BONGO_UI_READ_UNITS,
    BONGO_UI_READ_REPOSITORY, BONGO_UI_READ_INSTALLED, BONGO_UI_READ_IMAGE
};
enum BongoCatPluginUiAction {
    BONGO_UI_IMPORT, BONGO_UI_SAVE, BONGO_UI_OPEN_FOLDER, BONGO_UI_UPDATE_ITEM,
    BONGO_UI_DUPLICATE_ITEM, BONGO_UI_REMOVE_ITEM, BONGO_UI_PREVIEW_SOUND,
    BONGO_UI_CLEAR_SOUND, BONGO_UI_RESET_STATS, BONGO_UI_EXPORT_STATS,
    BONGO_UI_OVERLAY, BONGO_UI_USAGE_ENABLE, BONGO_UI_EXPORT_USAGE,
    BONGO_UI_SET_UNITS, BONGO_UI_SET_REPOSITORY, BONGO_UI_REFRESH_STORE,
    BONGO_UI_OPEN_URL, BONGO_UI_DOWNLOAD, BONGO_UI_UNINSTALL
};
typedef struct BongoCatPluginUiStatus {
    uint32_t busy, blocked, status, count, operation, operation_index;
    uint64_t received, total;
    uint32_t overlay, reset_available, reset_busy;
} BongoCatPluginUiStatus;
typedef struct BongoCatPluginPageUiServices {
    uint32_t abi_version, struct_size;
    void *context;
    uint32_t (*read)(void *, uint32_t, uint32_t, void *, uint32_t);
    uint32_t (*action)(void *, uint32_t, uint32_t, const void *, uint32_t);
} BongoCatPluginPageUiServices;
typedef struct BongoCatPluginPageUiContribution {
    uint32_t abi_version, struct_size;
    void *(*create)(void);
    void (*destroy)(void *);
    uint32_t (*draw)(void *, BongoCatPluginUi *, const BongoCatPluginPageUiHost *,
        const BongoCatPluginPageUiServices *);
} BongoCatPluginPageUiContribution;
typedef const BongoCatPluginPageUiContribution *(*BongoCatPluginPageUiQuery)(uint32_t);

#ifdef __cplusplus
}
#endif

#endif
