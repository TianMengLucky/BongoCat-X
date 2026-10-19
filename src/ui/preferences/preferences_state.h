#ifndef BONGO_CAT_PREFERENCES_STATE_H
#define BONGO_CAT_PREFERENCES_STATE_H

#include "preferences_internal.h"
#include "preferences_scrollbar.h"
#include "preferences_controls.h"
#include "bongo_cat/safe_ffi.h"
#include "preferences_text_session.h"
#include "about/preferences_about.h"
#include "ui_backend.h"

typedef struct BongoCatLive2DSetup BongoCatLive2DSetup;

#define BONGO_CAT_MODEL_LOAD_VISUAL_DURATION_NS 5000000000ull
#define BONGO_CAT_MODEL_LOAD_VISUAL_RAMP_NS 3000000000ull
#define BONGO_CAT_MODEL_LOAD_VISUAL_COMPLETE_NS 160000000ull
#define BONGO_CAT_MODEL_LOAD_VISUAL_WAIT_CAP 0.95f
#include "bongo_cat/i18n.h"
#include "bongo_cat/preferences.h"
#include "modal_frame.h"

typedef struct BongoCatPreferenceNotice {
    char message[1024];
    uint64_t started_ns;
    uint64_t until_ns;
    uint64_t closing_ns;
    uint64_t timer_updated_ns;
    struct nk_rect bounds;
    bool hovered;
    bool error;
    bool bottom_right;
    bool top_right;
    bool update_action;
    struct nk_rect close_bounds;
    struct nk_rect action_bounds;
} BongoCatPreferenceNotice;

struct BongoCatPreferences {
    BongoCatApp *app;
    SDL_Window *window;
    SDL_GLContext gl_context;
    bool owns_gl_context;
    bool transparent_window;
    bool rounded_window;
    bool visible;
    bool sdk_notice_shown;
    unsigned release_wait_flags;
    bool ui_initialized;
    BongoCatUIBackend ui;
    unsigned int logo_texture;
    unsigned int qq_texture;
    bool qq_attempted;
    int qq_width, qq_height;
    unsigned int asset_retry_count;
    uint64_t asset_retry_ns;
    unsigned int icon_texture;
    unsigned int icon_texture_hidpi;
    bool icon_hidpi_attempted;
    int icon_hidpi_cell;
    int logo_width;
    int logo_height;
    BongoCatAboutState about;
    struct nk_user_font support_logs_font;
    int page;
    enum nk_collapse_states options_sections[5];
    float quality_reload_from;
    uint64_t quality_settle_deadline_ns;
    /* UTF-8 edit contents retained for the host font atlas, independent of plugin state. */
    char plugin_edit_ids[8][64];
    char plugin_edit_texts[8][4096];
    uint32_t plugin_edit_cursor;
    int style_theme;
    uint32_t theme_colors[4];
    BongoCatLanguage font_language;
    nk_rune glyph_ranges[16384];
    nk_rune heading_glyph_ranges[512];
    uint64_t behavior_font_serial;
    bool input_active;
    bool import_requested;
    bool import_drop_active;
    bool import_render_active;
    BongoCatImportDialog *import_dialog;
    /* Runtime Live2D Core import: file picker plus its completion event. */
    int sdk_import_event_type;
    int live2d_setup_event_type;
    BongoCatLive2DSetup *live2d_setup;
    /* Background Live2D Core drop-in rescan (zip extraction can take a
       while): worker thread plus its completion event. */
    SDL_Thread *live2d_rescan_worker;
    int live2d_rescan_event_type;
    bool frame_checked;
    bool render_dirty;
    bool font_reload_pending;
    bool font_reload_defer_once;
    size_t pending_import_name_count;
    char pending_import_names[BONGO_CAT_MODEL_CAP][BONGO_CAT_ID_CAP];
    bool model_directory_watch_active;
    bool model_directory_watch_known;
    bool model_directory_watch_refresh;
    bool model_directory_watch_available;
    size_t model_directory_watch_count;
    uint64_t model_directory_watch_sum;
    uint64_t model_directory_watch_xor;
    uint64_t model_directory_watch_due_ns;
    bool smoke_behavior_open_pending;
    bool model_show_hidden;
    bool model_selection_pending;
    /* Model-dialog drag-reorder: the pressed row and the row being dragged
       (both catalog indices, -1 when idle), plus where the press started so
       a click on the toggle can be told apart from a drag. */
    int model_press_index;
    int model_drag_source;
    struct nk_vec2 model_press_point;
    /* Color picker debounce: timestamp of the latest drag change. The window
       repaints once the color has stayed stable for a short while instead of
       on every mouse move. */
    uint64_t color_picker_pending_ns;
    BongoCatPrefColorState theme_color_picker[4];
    BongoCatPrefColorState obs_color_picker;
    bool pending_model_multiple;
    bool pending_model_active;
    bool model_loading;
    bool model_load_visual_active;
    float model_load_progress;
    float model_load_render_progress;
    uint64_t model_load_render_ns;
    uint64_t model_load_render_cost_ns;
    uint64_t model_load_render_total_ns;
    unsigned model_load_render_count;
    unsigned model_load_render_deferred;
    uint64_t model_load_visual_started_ns;
    uint64_t model_load_visual_completion_ns;
    char pending_model_id[BONGO_CAT_ID_CAP];
    char pending_model_replace_id[BONGO_CAT_ID_CAP];
    char loading_model_id[BONGO_CAT_ID_CAP];
    char model_load_visual_id[BONGO_CAT_ID_CAP];
    uint64_t last_render_ns;
    uint64_t shown_ns;
    uint64_t fade_started_ns;
    bool fade_closing;
    float pending_raster_scale;
    uint64_t raster_retry_ns;
    uint64_t render_retry_ns;
    float scroll_current[13];
    float scroll_target[13];
    bool scroll_ready[13];
    bool page_seen;
    int last_page;
    uint64_t page_transition_ns;
    BongoCatPreferenceNotice notices[4];
    bool notice_pointer_captured;
    BongoCatBehaviorCatalog *behavior_catalog;
    char behavior_model_id[BONGO_CAT_ID_CAP];
    bool behavior_dialog;
    bool behavior_dialog_input_armed;
    int behavior_tab;
    uint64_t behavior_dialog_opened_ns;
    uint64_t behavior_dialog_closing_ns;
    uint64_t behavior_tab_transition_ns;
    float behavior_scroll[3];
    BongoCatPreferencesScrollbar behavior_scrollbar;
    BongoCatPreferencesTextSession behavior_rename;
    BongoCatPreferencesTextSession model_rename;
    /* Random expression/motion picker: rows with per-entry toggles. */
    bool random_dialog;
    BongoCatBehaviorKind random_dialog_kind;
    bool sequential_model_dialog;
    bool random_dialog_input_armed;
    uint64_t random_dialog_opened_ns;
    uint64_t random_dialog_closing_ns;
    float random_dialog_scroll;
    BongoCatPreferencesScrollbar random_dialog_scrollbar;
    /* Multi-key layout editor: the active side's hand-layout.json crop
       regions. Edits write the file after a short debounce, so dragging a
       stepper does not rewrite it every frame. */
    bool layout_dialog;
    bool layout_dialog_input_armed;
    bool layout_dialog_right;
    uint64_t layout_dialog_opened_ns;
    uint64_t layout_dialog_closing_ns;
    uint64_t layout_dialog_write_ns;
    float layout_dialog_scroll;
    BongoCatPreferencesScrollbar layout_dialog_scrollbar;
    char layout_dialog_directory[BONGO_CAT_PATH_CAP];
    BongoSafeHandLayout layout_dialog_draft;
    bool native_drag;
    bool chrome_dragging;
    bool live_resize_active;
    bool live_resize_rendering;
    bool live_resize_pending;
    bool live_resize_timer;
    bool live_resize_modal_ready;
    BongoCatModalFrame live_resize_modal_frame;
    unsigned live_resize_layout_frames;
    int drag_window_x;
    int drag_window_y;
    float drag_pointer_x;
    float drag_pointer_y;
    char shortcut_id[BONGO_CAT_BEHAVIOR_ID_CAP + 16];
    char *shortcut_target;
    int shortcut_capacity;
    char shortcut_original[BONGO_CAT_SHORTCUT_CAP];
    SDL_Keycode shortcut_key;
    bool shortcut_recording;
    uint64_t shortcut_suppress_until_ns;
#ifdef __APPLE__
    /* The permission changes while the user is in System Settings, so the page
       reads it again when it becomes visible and when the window regains focus
       instead of querying macOS per frame. */
    bool input_monitoring_authorized;
    bool input_monitoring_valid;
#endif
};

#ifdef __APPLE__
void bongo_cat_preferences_input_monitoring_refresh(BongoCatPreferences *value);
#endif

int bongo_cat_preferences_resolved_theme(const BongoCatPreferences *value);
void bongo_cat_preferences_apply_theme(BongoCatPreferences *value);
bool bongo_cat_preferences_open_window(BongoCatPreferences *value);
void bongo_cat_preferences_release_idle_window(BongoCatPreferences *value);
void bongo_cat_preferences_resource_note(BongoCatPreferences *value, const char *stage);
bool bongo_cat_preferences_scale_event(BongoCatPreferences *value,
    const SDL_Event *event);
bool bongo_cat_preferences_refresh_raster(BongoCatPreferences *value);
bool bongo_cat_preferences_reload_fonts(BongoCatPreferences *value);
bool bongo_cat_preferences_reload_language(BongoCatPreferences *value);
void bongo_cat_preferences_drag_tick(BongoCatPreferences *value);
void bongo_cat_preferences_live_resize_install(BongoCatPreferences *value);
void bongo_cat_preferences_live_resize_uninstall(BongoCatPreferences *value);
void bongo_cat_preferences_record_frame(BongoCatPreferences *value);
void bongo_cat_preferences_assets_load(BongoCatPreferences *value);
void bongo_cat_preferences_qq_load(BongoCatPreferences *value);
void bongo_cat_preferences_process_model_selection(BongoCatPreferences *value);
/* Pointer-input rows of the cat settings page (sensitivity, ignore-mouse and
   force-raw-mouse-input toggle), drawn from preferences_model.c to keep the
   page module within the source size policy. */
void bongo_cat_preferences_mouse_input_rows(BongoCatApp *app,
    BongoCatModelPreferences *model, struct nk_context *context);
/* Model-section rows of the cat settings page (gamepad four-hand mode and
   multi-key display, plus the layout editor entry). */
void bongo_cat_preferences_model_hands_rows(BongoCatPreferences *value,
    struct nk_context *context);
/* Render backend row of the general page (hot-switched via the app loop). */
void bongo_cat_preferences_mouse_range_row(BongoCatApp *app, struct nk_context *context);
void bongo_cat_preferences_memory_row(BongoCatApp *app, struct nk_context *context);
void bongo_cat_preferences_large_render_row(BongoCatApp *app,
    BongoCatApplicationPreferences *options, struct nk_context *context);
void bongo_cat_preferences_render_backend_row(BongoCatApp *app,
    BongoCatApplicationPreferences *options, struct nk_context *context);
void bongo_cat_preferences_model_visual_begin(BongoCatPreferences *value,
    const char *model_id);
void bongo_cat_preferences_model_load_progress(BongoCatPreferences *value,
    float progress);
bool bongo_cat_preferences_model_texture_busy(const BongoCatPreferences *value);
float bongo_cat_preferences_model_visual_progress(BongoCatPreferences *value,
    const char *model_id);
void bongo_cat_preferences_assets_clear(BongoCatPreferences *value);
void bongo_cat_preferences_assets_abandon(BongoCatPreferences *value);
void bongo_cat_preferences_model_cover_cache_clear(BongoCatApp *app);
void bongo_cat_preferences_smoke_frame(BongoCatPreferences *value);
void bongo_cat_preferences_icon_draw(BongoCatPreferences *value,
    struct nk_command_buffer *canvas, int icon, struct nk_rect bounds,
    struct nk_color color);

#endif
