#ifndef BONGO_CAT_CONFIG_H
#define BONGO_CAT_CONFIG_H

#include "bongo_cat/common.h"

#define BONGO_CAT_DEFAULT_MAX_FPS 60
#define BONGO_CAT_DEFAULT_RENDER_QUALITY_PERCENT 100
/* Resolve this saved choice using the display refresh rate cached at startup. */
#define BONGO_CAT_DISPLAY_MAX_FPS (-1)
#define BONGO_CAT_DEFAULT_WINDOW_SCALE_PERCENT 100.0f
#define BONGO_CAT_DEFAULT_WINDOW_OPACITY_PERCENT 100.0f
#define BONGO_CAT_DEFAULT_RANDOM_EXPRESSION_SECONDS 5.0f
#define BONGO_CAT_DEFAULT_RANDOM_MOTION_SECONDS 5.0f
/* Switching models reloads textures; a much longer default avoids churn. */
#define BONGO_CAT_DEFAULT_SEQUENTIAL_MODEL_SECONDS 600.0f
#define BONGO_CAT_DEFAULT_WINDOW_CORNER_PERCENT 6.0f
#define BONGO_CAT_DEFAULT_HIDE_FADE_SECONDS 0.3f
#define BONGO_CAT_MAX_HIDE_FADE_SECONDS 3.0f

typedef enum BongoCatTheme { BONGO_CAT_THEME_AUTO, BONGO_CAT_THEME_LIGHT, BONGO_CAT_THEME_DARK } BongoCatTheme;
typedef enum BongoCatLanguage {
    BONGO_CAT_LANG_EN_US,
    BONGO_CAT_LANG_ZH_CN,
    BONGO_CAT_LANG_ZH_HANT,
    BONGO_CAT_LANG_FR_FR,
    BONGO_CAT_LANG_DE_DE,
    BONGO_CAT_LANG_JA_JP,
    BONGO_CAT_LANG_KO_KR,
    BONGO_CAT_LANG_PT_BR,
    BONGO_CAT_LANG_RU_RU,
    BONGO_CAT_LANG_ES_ES,
    BONGO_CAT_LANG_COUNT
} BongoCatLanguage;
typedef enum BongoCatModelMode {
    BONGO_CAT_MODE_STANDARD,
    BONGO_CAT_MODE_KEYBOARD,
    BONGO_CAT_MODE_GAMEPAD
} BongoCatModelMode;
/* OBS solid background key color, 0x00RRGGBB, chosen with the preferences
   color picker (saturation/value square plus hue slider). */
#define BONGO_CAT_DEFAULT_OBS_BACKGROUND_RGB 0x00ff00u

typedef struct BongoCatModelPreferences {
    bool multiple_pets;
    bool mirror;
    bool vertical_flip;
    bool mouse_mirror;
    bool mouse_vertical_flip;
    bool mouse_centered;
    bool ignore_mouse;
    /* Windows only: drive the model from Raw Input device motion without
       waiting for the cursor-lock detector (games that hide the pointer). */
    bool force_mouse_input;
    bool gamepad_four_hands;
    bool dynamic_texture_resolution;
    /* Approximate texture-memory budget: 0.1, 1, then 10 to 100 percent. */
    float render_quality_percent;
    int max_fps;
} BongoCatModelPreferences;

typedef struct BongoCatWindowPreferences {
    bool pass_through;
    bool always_on_top;
    bool hide_on_hover;
    bool keep_in_screen;
    bool edge_snap;
    /*
     * 只在录屏/直播软件里可见: 桌面上看不见这个窗口, 但 OBS 之类的采集
     * (窗口采集 WGC / 游戏采集) 仍然拿得到画面。Windows 上通过 DWM 隐藏
     * (DWMWA_CLOAK) 实现, 其它平台暂不支持。
     */
    bool capture_only;
    bool obs_background;
    /* 窗口边框紧贴桌宠可视边缘，减少透明边距。 */
    bool tight_frame;
    bool random_expression;
    bool random_motion;
    /* Walks the model catalog in the user's drag order. */
    bool sequential_model;
    bool rounded_corners;
    uint32_t obs_background_rgb;
    float hide_delay_seconds;
    float hide_fade_seconds;
    float random_expression_interval_seconds;
    float random_motion_interval_seconds;
    float sequential_model_interval_seconds;
    float corner_radius_percent;
} BongoCatWindowPreferences;

typedef struct BongoCatWindowState {
    bool visible;
    bool position_known;
    float scale_percent;
    float opacity_percent;
    int x;
    int y;
    int width;
    int height;
    /* The authored composition size inside width/height.  The outer window
       may be larger to hold animated geometry outside the base canvas. */
    int content_width;
    int content_height;
    /* Physical top-left inset, retained so resetting the learned motion frame
       at startup does not move the content origin on the desktop. */
    int content_left;
    int content_top;
} BongoCatWindowState;

typedef struct BongoCatApplicationPreferences {
    bool autostart;
    /* Relaunch elevated; the registry autostart entry is independent of it. */
    bool run_as_admin;
    bool tray_visible;
    BongoCatTheme theme;
    BongoCatLanguage language;
} BongoCatApplicationPreferences;

typedef struct BongoCatShortcutPreferences {
    char toggle_pet_visibility[BONGO_CAT_SHORTCUT_CAP];
    char visible_preferences[BONGO_CAT_SHORTCUT_CAP];
    char open_menu[BONGO_CAT_SHORTCUT_CAP];
    char mirror[BONGO_CAT_SHORTCUT_CAP];
    char pass_through[BONGO_CAT_SHORTCUT_CAP];
    char always_on_top[BONGO_CAT_SHORTCUT_CAP];
} BongoCatShortcutPreferences;

typedef struct BongoCatBehaviorShortcut {
    char id[BONGO_CAT_BEHAVIOR_ID_CAP];
    char shortcut[BONGO_CAT_SHORTCUT_CAP];
    char label[BONGO_CAT_ID_CAP];
    bool shortcut_disabled;
    /* Runtime only: the shortcut is owned by the model's Mver config. */
    bool shortcut_external;
} BongoCatBehaviorShortcut;

typedef struct BongoCatModelLabel {
    char id[BONGO_CAT_ID_CAP];
    char label[BONGO_CAT_ID_CAP];
} BongoCatModelLabel;

#define BONGO_CAT_RANDOM_DISABLED_CAP 256

typedef struct BongoCatRemovedModel {
    char id[BONGO_CAT_ID_CAP];
} BongoCatRemovedModel;

typedef struct BongoCatActiveBehavior {
    char model_id[BONGO_CAT_ID_CAP];
    char behavior_id[BONGO_CAT_BEHAVIOR_ID_CAP];
} BongoCatActiveBehavior;

#define BONGO_CAT_SETTINGS_EXTENSIONS_CAP 4096

typedef struct BongoCatSettings {
    BongoCatModelPreferences model;
    BongoCatWindowPreferences window;
    BongoCatApplicationPreferences app;
    BongoCatShortcutPreferences shortcuts;
    BongoCatBehaviorShortcut behavior_shortcuts[BONGO_CAT_BEHAVIOR_BINDING_CAP];
    size_t behavior_shortcut_count;
    /* Behavior ids excluded from random expression/motion selection. */
    char random_disabled[BONGO_CAT_RANDOM_DISABLED_CAP][BONGO_CAT_BEHAVIOR_ID_CAP];
    size_t random_disabled_count;
    BongoCatModelLabel model_labels[BONGO_CAT_MODEL_CAP];
    size_t model_label_count;
    BongoCatRemovedModel removed_models[BONGO_CAT_MODEL_CAP];
    size_t removed_model_count;
    BongoCatRemovedModel hidden_models[BONGO_CAT_MODEL_CAP];
    size_t hidden_model_count;
    /* Model ids in the user's drag order; unlisted models follow in scan
       order. Drives the model page layout and sequential model switching. */
    BongoCatRemovedModel model_order[BONGO_CAT_MODEL_CAP];
    size_t model_order_count;
    char extensions_json[BONGO_CAT_SETTINGS_EXTENSIONS_CAP];
} BongoCatSettings;

typedef struct BongoCatSessionState {
    BongoCatWindowState window;
    char active_model_id[BONGO_CAT_ID_CAP];
    int last_update_check_day;
    char last_update_check_version[BONGO_CAT_UPDATE_VERSION_CAP];
    char available_update_version[BONGO_CAT_UPDATE_VERSION_CAP];
    char additional_model_ids[BONGO_CAT_ADDITIONAL_MODEL_CAP][BONGO_CAT_ID_CAP];
    size_t additional_model_count;
    BongoCatActiveBehavior active_behaviors[BONGO_CAT_BEHAVIOR_BINDING_CAP];
    size_t active_behavior_count;
} BongoCatSessionState;

#ifdef __cplusplus
extern "C" {
#endif

void bongo_cat_settings_defaults(BongoCatSettings *settings);
void bongo_cat_settings_validate(BongoCatSettings *settings);
/* Snaps a render quality percent to the nearest valid level
   (0.1, 1, 10, 20, ... 100). */
float bongo_cat_settings_snap_render_quality(float percent);
void bongo_cat_session_defaults(BongoCatSessionState *session);
void bongo_cat_session_validate(BongoCatSessionState *session);
bool bongo_cat_session_model_active(const BongoCatSessionState *session,
    const char *model_id);
bool bongo_cat_session_add_model(BongoCatSessionState *session,
    const char *model_id);
bool bongo_cat_session_remove_model(BongoCatSessionState *session,
    const char *model_id);
void bongo_cat_session_clear_additional_models(BongoCatSessionState *session);
bool bongo_cat_settings_shortcut_conflicts(const BongoCatSettings *settings,
    const char *shortcut, const char *exclude);
const char *bongo_cat_settings_model_label(const BongoCatSettings *settings,
    const char *id);
bool bongo_cat_settings_set_model_label(BongoCatSettings *settings,
    const char *id, const char *label);
bool bongo_cat_settings_model_removed(const BongoCatSettings *settings,
    const char *id);
bool bongo_cat_settings_set_model_removed(BongoCatSettings *settings,
    const char *id, bool removed);
bool bongo_cat_settings_model_hidden(const BongoCatSettings *settings,
    const char *id);
bool bongo_cat_settings_set_model_hidden(BongoCatSettings *settings,
    const char *id, bool hidden);
/* 1-based drag position of a model, or 0 when the model is unlisted. */
size_t bongo_cat_settings_model_order_index(const BongoCatSettings *settings,
    const char *id);
/* Replaces the stored order with the given model ids (up to the cap). */
bool bongo_cat_settings_model_order_set(BongoCatSettings *settings,
    const char *const *ids, size_t count);
bool bongo_cat_settings_restore_model_package(BongoCatSettings *settings,
    const char *package_id);
bool bongo_cat_settings_random_enabled(const BongoCatSettings *settings,
    const char *behavior_id);
bool bongo_cat_settings_random_set_enabled(BongoCatSettings *settings,
    const char *behavior_id, bool enabled);
BongoCatResult bongo_cat_settings_load(const char *path,
    BongoCatSettings *settings, BongoCatError *error);
BongoCatResult bongo_cat_settings_save(const char *path,
    const BongoCatSettings *settings, BongoCatError *error);
BongoCatResult bongo_cat_session_load(const char *path,
    BongoCatSessionState *session, BongoCatError *error);
BongoCatResult bongo_cat_session_save(const char *path,
    const BongoCatSessionState *session, BongoCatError *error);
const char *bongo_cat_theme_name(BongoCatTheme value);
const char *bongo_cat_language_name(BongoCatLanguage value);
bool bongo_cat_language_parse(const char *name, BongoCatLanguage *value);
bool bongo_cat_language_from_locale(const char *language,
    const char *country, BongoCatLanguage *value);
const char *bongo_cat_mode_name(BongoCatModelMode value);

#ifdef __cplusplus
}
#endif

#endif
