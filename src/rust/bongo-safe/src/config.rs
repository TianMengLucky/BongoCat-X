//! Configuration JSON over the C ABI, replacing the former C parser-based field
//! readers in `config_io.c` / `config_session_io.c` and the writers in
//! `config_settings_save.c` / `config_session_io.c`.
//!
//! The C side owns the files (size cap, atomic replace, fsync) and the
//! struct definitions in `bongo_cat/config.h`; this module mirrors those
//! structs `#[repr(C)]` behind runtime size checks and converts between
//! them and JSON with the same tolerant semantics as the C readers:
//! every field is optional (the caller passes pre-defaulted structs),
//! wrong types are format errors, and legacy keys migrate.

use crate::unique::UniqueValue;
use serde_json::{Map, Value};
use std::os::raw::{c_char, c_int};

const ID_CAP: usize = 128;
const PATH_CAP: usize = 1024;
const SHORTCUT_CAP: usize = 128;
const MODEL_CAP: usize = 128;
const ADDITIONAL_MODEL_CAP: usize = 7;
const BEHAVIOR_ID_CAP: usize = 384;
const BEHAVIOR_BINDING_CAP: usize = 256;
const RANDOM_DISABLED_CAP: usize = 256;
const UPDATE_VERSION_CAP: usize = 32;
const EXTENSIONS_CAP: usize = 4096;

const OK: c_int = 0;
const FORMAT: c_int = 3;
const MEMORY: c_int = 4;
const LAYOUT: c_int = -1;

const SETTINGS_FORMAT: &str = "bongocat/settings";
const SESSION_FORMAT: &str = "bongocat/session";
const SCHEMA_VERSION: i64 = 1;

#[repr(C)]
struct ModelPreferences {
    multiple_pets: bool,
    mirror: bool,
    vertical_flip: bool,
    mouse_mirror: bool,
    mouse_vertical_flip: bool,
    mouse_centered: bool,
    ignore_mouse: bool,
    force_mouse_input: bool,
    gamepad_four_hands: bool,
    dynamic_texture_resolution: bool,
    render_quality_percent: f32,
    max_fps: c_int,
}

#[repr(C)]
struct WindowPreferences {
    pass_through: bool,
    always_on_top: bool,
    hide_on_hover: bool,
    keep_in_screen: bool,
    edge_snap: bool,
    capture_only: bool,
    obs_background: bool,
    tight_frame: bool,
    random_expression: bool,
    random_motion: bool,
    sequential_model: bool,
    rounded_corners: bool,
    obs_background_rgb: u32,
    hide_delay_seconds: f32,
    hide_fade_seconds: f32,
    random_expression_interval_seconds: f32,
    random_motion_interval_seconds: f32,
    sequential_model_interval_seconds: f32,
    corner_radius_percent: f32,
    custom_background: bool,
    custom_background_path: [c_char; PATH_CAP],
}

#[repr(C)]
struct ApplicationPreferences {
    autostart: bool,
    run_as_admin: bool,
    tray_visible: bool,
    large_render_optimization: bool,
    render_parallel_recording: bool,
    render_bindless: bool,
    render_large_allocation: bool,
    theme: c_int,
    language: c_int,
    render_backend: c_int,
}

#[repr(C)]
struct ShortcutPreferences {
    toggle_pet_visibility: [c_char; SHORTCUT_CAP],
    visible_preferences: [c_char; SHORTCUT_CAP],
    open_menu: [c_char; SHORTCUT_CAP],
    mirror: [c_char; SHORTCUT_CAP],
    pass_through: [c_char; SHORTCUT_CAP],
    always_on_top: [c_char; SHORTCUT_CAP],
}

#[repr(C)]
struct BehaviorShortcut {
    id: [c_char; BEHAVIOR_ID_CAP],
    shortcut: [c_char; SHORTCUT_CAP],
    label: [c_char; ID_CAP],
    shortcut_disabled: bool,
    shortcut_external: bool,
}

#[repr(C)]
struct ModelLabel {
    id: [c_char; ID_CAP],
    label: [c_char; ID_CAP],
}

#[repr(C)]
struct RemovedModel {
    id: [c_char; ID_CAP],
}

#[repr(C)]
struct ActiveBehavior {
    model_id: [c_char; ID_CAP],
    behavior_id: [c_char; BEHAVIOR_ID_CAP],
}

#[repr(C)]
pub struct Settings {
    model: ModelPreferences,
    window: WindowPreferences,
    app: ApplicationPreferences,
    shortcuts: ShortcutPreferences,
    behavior_shortcuts: [BehaviorShortcut; BEHAVIOR_BINDING_CAP],
    behavior_shortcut_count: usize,
    random_disabled: [[c_char; BEHAVIOR_ID_CAP]; RANDOM_DISABLED_CAP],
    random_disabled_count: usize,
    model_labels: [ModelLabel; MODEL_CAP],
    model_label_count: usize,
    removed_models: [RemovedModel; MODEL_CAP],
    removed_model_count: usize,
    hidden_models: [RemovedModel; MODEL_CAP],
    hidden_model_count: usize,
    model_order: [RemovedModel; MODEL_CAP],
    model_order_count: usize,
    extensions_json: [c_char; EXTENSIONS_CAP],
}

#[repr(C)]
struct WindowState {
    visible: bool,
    position_known: bool,
    scale_percent: f32,
    opacity_percent: f32,
    x: c_int,
    y: c_int,
    width: c_int,
    height: c_int,
    content_width: c_int,
    content_height: c_int,
    content_left: c_int,
    content_top: c_int,
}

#[repr(C)]
pub struct SessionState {
    window: WindowState,
    active_model_id: [c_char; ID_CAP],
    last_update_check_day: c_int,
    last_update_check_version: [c_char; UPDATE_VERSION_CAP],
    available_update_version: [c_char; UPDATE_VERSION_CAP],
    additional_model_ids: [[c_char; ID_CAP]; ADDITIONAL_MODEL_CAP],
    additional_model_count: usize,
    active_behaviors: [ActiveBehavior; BEHAVIOR_BINDING_CAP],
    active_behavior_count: usize,
}

/// Accumulates the format message for the C-side `BongoCatError`.
#[derive(Debug)]
struct Failure {
    code: c_int,
    message: String,
}

impl Failure {
    fn format(message: String) -> Self {
        Failure {
            code: FORMAT,
            message,
        }
    }

    fn memory(message: String) -> Self {
        Failure {
            code: MEMORY,
            message,
        }
    }
}

type Outcome<T> = Result<T, Failure>;

fn object<'a>(
    parent: &'a Map<String, Value>,
    description: &str,
    key: &str,
) -> Outcome<Option<&'a Map<String, Value>>> {
    match parent.get(key) {
        None | Some(Value::Null) => Ok(None),
        Some(Value::Object(object)) => Ok(Some(object)),
        Some(_) => Err(Failure::format(format!(
            "{description} field '{key}' must be an object"
        ))),
    }
}

fn array<'a>(
    parent: &'a Map<String, Value>,
    description: &str,
    key: &str,
) -> Outcome<Option<&'a Vec<Value>>> {
    match parent.get(key) {
        None | Some(Value::Null) => Ok(None),
        Some(Value::Array(array)) => Ok(Some(array)),
        Some(_) => Err(Failure::format(format!(
            "{description} field '{key}' must be an array"
        ))),
    }
}

fn string<'a>(
    parent: &'a Map<String, Value>,
    description: &str,
    key: &str,
) -> Outcome<Option<&'a str>> {
    match parent.get(key) {
        None | Some(Value::Null) => Ok(None),
        Some(Value::String(text)) => {
            if text.bytes().any(|byte| byte == 0) {
                return Err(Failure::format(format!(
                    "{description} field '{key}' must be a string without embedded nulls"
                )));
            }
            Ok(Some(text.as_str()))
        }
        Some(_) => Err(Failure::format(format!(
            "{description} field '{key}' must be a string"
        ))),
    }
}

fn boolean(
    parent: &Map<String, Value>,
    description: &str,
    key: &str,
    target: &mut bool,
) -> Outcome<()> {
    match parent.get(key) {
        None | Some(Value::Null) => Ok(()),
        Some(Value::Bool(value)) => {
            *target = *value;
            Ok(())
        }
        Some(_) => Err(Failure::format(format!(
            "{description} field '{key}' must be a boolean"
        ))),
    }
}

fn integer(
    parent: &Map<String, Value>,
    description: &str,
    key: &str,
    target: &mut c_int,
) -> Outcome<()> {
    let number = match parent.get(key) {
        None | Some(Value::Null) => return Ok(()),
        Some(value) => value,
    };
    let converted = match number {
        Value::Number(number) => number
            .as_i64()
            .filter(|value| *value >= i32::MIN as i64 && *value <= i32::MAX as i64)
            .or_else(|| {
                number
                    .as_u64()
                    .and_then(|value| u32::try_from(value).ok().map(|v| v as i64))
            })
            .map(|value| value as c_int),
        _ => None,
    };
    match converted {
        Some(value) => {
            *target = value;
            Ok(())
        }
        None => Err(Failure::format(format!(
            "{description} field '{key}' must be an integer in the supported range"
        ))),
    }
}

fn real(
    parent: &Map<String, Value>,
    description: &str,
    key: &str,
    target: &mut f32,
) -> Outcome<()> {
    let number = match parent.get(key) {
        None | Some(Value::Null) => return Ok(()),
        Some(value) => value,
    };
    match number.as_f64() {
        Some(value)
            if value.is_finite() && value >= -f32::MAX as f64 && value <= f32::MAX as f64 =>
        {
            *target = value as f32;
            Ok(())
        }
        _ => Err(Failure::format(format!(
            "{description} field '{key}' must be a finite number in the supported range"
        ))),
    }
}

fn text(
    parent: &Map<String, Value>,
    description: &str,
    key: &str,
    target: &mut [c_char],
) -> Outcome<()> {
    let value = match string(parent, description, key)? {
        Some(value) => value,
        None => return Ok(()),
    };
    copy_text(description, target, value, key)
}

fn copy_text(description: &str, target: &mut [c_char], value: &str, field: &str) -> Outcome<()> {
    if value.len() >= target.len() {
        return Err(Failure::format(format!(
            "{description} field '{field}' must be a shorter string"
        )));
    }
    for byte in target.iter_mut() {
        *byte = 0;
    }
    for (index, byte) in value.bytes().enumerate() {
        target[index] = byte as c_char;
    }
    Ok(())
}

fn c_string(field: &[c_char]) -> String {
    let bytes: Vec<u8> = field
        .iter()
        .take_while(|byte| **byte != 0)
        .map(|byte| *byte as u8)
        .collect();
    String::from_utf8_lossy(&bytes).into_owned()
}

fn theme_from_name(value: &str) -> Option<c_int> {
    match value {
        "auto" => Some(0),
        "light" => Some(1),
        "dark" => Some(2),
        _ => None,
    }
}

fn language_from_name(value: &str) -> Option<c_int> {
    // Mirrors bongo_cat_language_parse, including the legacy zh-TW alias.
    let names = [
        "en-US", "zh-CN", "zh-Hant", "fr-FR", "de-DE", "ja-JP", "ko-KR", "pt-BR", "ru-RU", "es-ES",
    ];
    if value == "zh-TW" {
        return Some(2);
    }
    names
        .iter()
        .position(|name| *name == value)
        .map(|index| index as c_int)
}

fn render_backend_from_name(value: &str) -> Option<c_int> {
    // Mirrors bongo_cat_render_backend_parse.
    let names = ["auto", "opengl", "vulkan", "metal"];
    names
        .iter()
        .position(|name| *name == value)
        .map(|index| index as c_int)
}

fn background_color_from_text(value: &str) -> Option<u32> {
    let bytes = value.as_bytes();
    if bytes.len() != 7 || bytes[0] != b'#' {
        return None;
    }
    let mut rgb = 0u32;
    for byte in &bytes[1..] {
        let digit = match byte {
            b'0'..=b'9' => byte - b'0',
            b'a'..=b'f' => byte - b'a' + 10,
            b'A'..=b'F' => byte - b'A' + 10,
            _ => return None,
        };
        rgb = (rgb << 4) | digit as u32;
    }
    Some(rgb)
}

fn read_model(object: &Map<String, Value>, value: &mut ModelPreferences) -> Outcome<()> {
    const DESCRIPTION: &str = "Settings";
    boolean(
        object,
        DESCRIPTION,
        "multiplePets",
        &mut value.multiple_pets,
    )?;
    boolean(object, DESCRIPTION, "modelMirrored", &mut value.mirror)?;
    boolean(
        object,
        DESCRIPTION,
        "modelFlippedVertically",
        &mut value.vertical_flip,
    )?;
    boolean(
        object,
        DESCRIPTION,
        "pointerMirrored",
        &mut value.mouse_mirror,
    )?;
    boolean(
        object,
        DESCRIPTION,
        "pointerFlippedVertically",
        &mut value.mouse_vertical_flip,
    )?;
    boolean(
        object,
        DESCRIPTION,
        "centerPointerTracking",
        &mut value.mouse_centered,
    )?;
    boolean(
        object,
        DESCRIPTION,
        "ignorePointerInput",
        &mut value.ignore_mouse,
    )?;
    boolean(
        object,
        DESCRIPTION,
        "forceMouseInput",
        &mut value.force_mouse_input,
    )?;
    boolean(
        object,
        DESCRIPTION,
        "gamepadFourHands",
        &mut value.gamepad_four_hands,
    )?;
    boolean(
        object,
        DESCRIPTION,
        "dynamicTextureResolution",
        &mut value.dynamic_texture_resolution,
    )?;
    real(
        object,
        DESCRIPTION,
        "renderQualityPercent",
        &mut value.render_quality_percent,
    )?;
    integer(object, DESCRIPTION, "maximumFps", &mut value.max_fps)?;
    Ok(())
}

fn read_window(object: &Map<String, Value>, value: &mut WindowPreferences) -> Outcome<()> {
    boolean(object, "Settings", "customBackground", &mut value.custom_background)?;
    text(object, "Settings", "customBackgroundPath", &mut value.custom_background_path)?;
    value.custom_background &= value.custom_background_path[0] != 0;
    const DESCRIPTION: &str = "Settings";
    boolean(object, DESCRIPTION, "clickThrough", &mut value.pass_through)?;
    boolean(object, DESCRIPTION, "alwaysOnTop", &mut value.always_on_top)?;
    boolean(
        object,
        DESCRIPTION,
        "hideOnPointerOver",
        &mut value.hide_on_hover,
    )?;
    boolean(
        object,
        DESCRIPTION,
        "keepOnScreen",
        &mut value.keep_in_screen,
    )?;
    boolean(object, DESCRIPTION, "edgeSnap", &mut value.edge_snap)?;
    boolean(object, DESCRIPTION, "captureOnly", &mut value.capture_only)?;
    boolean(
        object,
        DESCRIPTION,
        "captureBackground",
        &mut value.obs_background,
    )?;
    boolean(object, DESCRIPTION, "tightFrame", &mut value.tight_frame)?;
    boolean(
        object,
        DESCRIPTION,
        "randomExpression",
        &mut value.random_expression,
    )?;
    boolean(
        object,
        DESCRIPTION,
        "randomMotion",
        &mut value.random_motion,
    )?;
    boolean(
        object,
        DESCRIPTION,
        "sequentialModel",
        &mut value.sequential_model,
    )?;
    /* Pre-sequential builds stored the switch under randomModel with a
    minutes interval; migrate both when the new keys are absent. */
    let legacy_random_model = matches!(object.get("randomModel"), Some(Value::Bool(true)));
    let mut sequential_model_interval_seconds = 0.0f32;
    let mut legacy_random_model_minutes = 0.0f32;
    real(
        object,
        DESCRIPTION,
        "sequentialModelIntervalSeconds",
        &mut sequential_model_interval_seconds,
    )?;
    real(
        object,
        DESCRIPTION,
        "randomModelIntervalMinutes",
        &mut legacy_random_model_minutes,
    )?;
    boolean(
        object,
        DESCRIPTION,
        "roundedCorners",
        &mut value.rounded_corners,
    )?;
    real(
        object,
        DESCRIPTION,
        "cornerRadiusPercent",
        &mut value.corner_radius_percent,
    )?;
    real(
        object,
        DESCRIPTION,
        "hideDelaySeconds",
        &mut value.hide_delay_seconds,
    )?;
    real(
        object,
        DESCRIPTION,
        "hideFadeSeconds",
        &mut value.hide_fade_seconds,
    )?;
    real(
        object,
        DESCRIPTION,
        "randomExpressionIntervalSeconds",
        &mut value.random_expression_interval_seconds,
    )?;
    real(
        object,
        DESCRIPTION,
        "randomMotionIntervalSeconds",
        &mut value.random_motion_interval_seconds,
    )?;
    /* A migrated value only applies when no new-format value was saved. */
    if sequential_model_interval_seconds > 0.0 {
        value.sequential_model_interval_seconds = sequential_model_interval_seconds;
    } else if legacy_random_model_minutes > 0.0 {
        value.sequential_model_interval_seconds = legacy_random_model_minutes * 60.0;
    }
    if legacy_random_model {
        value.sequential_model = true;
    }
    if value.custom_background {
        value.obs_background = false;
    }
    let color = string(object, DESCRIPTION, "captureBackgroundColor")?;
    if let Some(color) = color {
        match background_color_from_text(color) {
            Some(rgb) => value.obs_background_rgb = rgb,
            None => {
                return Err(Failure::format(
                    "Settings field 'captureBackgroundColor' must be a #rrggbb color string"
                        .to_string(),
                ))
            }
        }
    }
    Ok(())
}

fn read_app(object: &Map<String, Value>, value: &mut ApplicationPreferences) -> Outcome<()> {
    const DESCRIPTION: &str = "Settings";
    boolean(object, DESCRIPTION, "launchAtLogin", &mut value.autostart)?;
    boolean(object, DESCRIPTION, "runAsAdmin", &mut value.run_as_admin)?;
    /* Settings written before the option was renamed carried the same
    meaning under the old key; the new key wins when both are present. */
    let legacy_admin = matches!(object.get("gameCompatibility"), Some(Value::Bool(true)));
    boolean(object, DESCRIPTION, "showTrayIcon", &mut value.tray_visible)?;
    boolean(object, DESCRIPTION, "largeRenderOptimization", &mut value.large_render_optimization)?;
    // Older settings requested all features through the master switch.
    value.render_parallel_recording = true;
    boolean(object, DESCRIPTION, "largeRenderParallelRecording", &mut value.render_parallel_recording)?;
    value.render_bindless = true;
    boolean(object, DESCRIPTION, "largeRenderBindless", &mut value.render_bindless)?;
    value.render_large_allocation = true;
    boolean(object, DESCRIPTION, "largeRenderLargeAllocation", &mut value.render_large_allocation)?;
    if object.get("runAsAdmin").is_none() && legacy_admin {
        value.run_as_admin = true;
    }
    let theme = string(object, DESCRIPTION, "theme")?;
    if let Some(theme) = theme {
        match theme_from_name(theme) {
            Some(theme) => value.theme = theme,
            None => {
                return Err(Failure::format(
                    "Settings field 'theme' must be auto, light, or dark".to_string(),
                ))
            }
        }
    }
    let language = string(object, DESCRIPTION, "language")?;
    if let Some(language) = language {
        match language_from_name(language) {
            Some(language) => value.language = language,
            None => {
                return Err(Failure::format(
                    "Settings field 'language' must be a supported locale string".to_string(),
                ))
            }
        }
    }
    let render_backend = string(object, DESCRIPTION, "renderBackend")?;
    if let Some(render_backend) = render_backend {
        match render_backend_from_name(render_backend) {
            Some(render_backend) => value.render_backend = render_backend,
            None => {
                return Err(Failure::format(
                    "Settings field 'renderBackend' must be auto, opengl, vulkan, or metal".to_string(),
                ))
            }
        }
    }
    Ok(())
}

fn read_shortcuts(object: &Map<String, Value>, value: &mut ShortcutPreferences) -> Outcome<()> {
    const DESCRIPTION: &str = "Settings";
    text(
        object,
        DESCRIPTION,
        "toggleVisibility",
        &mut value.toggle_pet_visibility,
    )?;
    text(
        object,
        DESCRIPTION,
        "openSettings",
        &mut value.visible_preferences,
    )?;
    text(object, DESCRIPTION, "openMenu", &mut value.open_menu)?;
    text(object, DESCRIPTION, "toggleModelMirror", &mut value.mirror)?;
    text(
        object,
        DESCRIPTION,
        "toggleClickThrough",
        &mut value.pass_through,
    )?;
    text(
        object,
        DESCRIPTION,
        "toggleAlwaysOnTop",
        &mut value.always_on_top,
    )?;
    Ok(())
}

fn read_behavior_object(object: &Map<String, Value>, entry: &mut BehaviorShortcut) -> Outcome<()> {
    const DESCRIPTION: &str = "Settings";
    let id = string(object, DESCRIPTION, "behaviorId")?;
    let shortcut = string(object, DESCRIPTION, "shortcut")?;
    let label = string(object, DESCRIPTION, "displayName")?;
    let id = match id {
        Some(id) if !id.is_empty() => id,
        _ => {
            return Err(Failure::format(
                "Settings field 'behaviorId' must be a non-empty string".to_string(),
            ))
        }
    };
    boolean(
        object,
        DESCRIPTION,
        "shortcutDisabled",
        &mut entry.shortcut_disabled,
    )?;
    copy_text(DESCRIPTION, &mut entry.id, id, "behaviorId")?;
    if let Some(shortcut) = shortcut {
        copy_text(DESCRIPTION, &mut entry.shortcut, shortcut, "shortcut")?;
    }
    if let Some(label) = label {
        copy_text(DESCRIPTION, &mut entry.label, label, "displayName")?;
    }
    Ok(())
}

fn read_behaviors(array: Option<&Vec<Value>>, settings: &mut Settings) -> Outcome<()> {
    let Some(array) = array else { return Ok(()) };
    if array.len() > BEHAVIOR_BINDING_CAP {
        return Err(Failure::format(
            "Settings field 'behaviorOverrides' must be a smaller array".to_string(),
        ));
    }
    settings.behavior_shortcut_count = 0;
    for item in array {
        let object = match item {
            Value::Object(object) => object,
            _ => {
                return Err(Failure::format(
                    "Settings field 'behaviorOverrides[]' must be an object".to_string(),
                ))
            }
        };
        let entry = &mut settings.behavior_shortcuts[settings.behavior_shortcut_count];
        *entry = BehaviorShortcut {
            id: [0; BEHAVIOR_ID_CAP],
            shortcut: [0; SHORTCUT_CAP],
            label: [0; ID_CAP],
            shortcut_disabled: false,
            shortcut_external: false,
        };
        read_behavior_object(object, entry)?;
        settings.behavior_shortcut_count += 1;
    }
    Ok(())
}

fn read_random_disabled(array: Option<&Vec<Value>>, settings: &mut Settings) -> Outcome<()> {
    let Some(array) = array else { return Ok(()) };
    if array.len() > RANDOM_DISABLED_CAP {
        return Err(Failure::format(
            "Settings field 'randomBehaviorDisabled' must be a smaller array".to_string(),
        ));
    }
    settings.random_disabled_count = 0;
    for item in array {
        let id = match item {
            Value::String(id) if !id.is_empty() && !id.bytes().any(|byte| byte == 0) => id,
            _ => {
                return Err(Failure::format(
                    "Settings field 'randomBehaviorDisabled[]' must be a non-empty string without embedded nulls".to_string(),
                ))
            }
        };
        let entry = &mut settings.random_disabled[settings.random_disabled_count];
        entry.fill(0);
        copy_text("Settings", entry, id, "randomBehaviorDisabled[]")?;
        settings.random_disabled_count += 1;
    }
    Ok(())
}

fn read_model_labels(array: Option<&Vec<Value>>, settings: &mut Settings) -> Outcome<()> {
    let Some(array) = array else { return Ok(()) };
    if array.len() > MODEL_CAP {
        return Err(Failure::format(
            "Settings field 'modelOverrides' must be a smaller array".to_string(),
        ));
    }
    settings.model_label_count = 0;
    for item in array {
        let object = match item {
            Value::Object(object) => object,
            _ => {
                return Err(Failure::format(
                    "Settings field 'modelOverrides[]' must be an object".to_string(),
                ))
            }
        };
        let id = string(object, "Settings", "modelId")?;
        let label = string(object, "Settings", "displayName")?;
        let (id, label) = match (id, label) {
            (Some(id), Some(label)) if !id.is_empty() && !label.is_empty() => (id, label),
            _ => {
                return Err(Failure::format(
                    "Settings field 'modelOverrides[]' must be non-empty modelId and displayName strings".to_string(),
                ))
            }
        };
        let entry = &mut settings.model_labels[settings.model_label_count];
        *entry = ModelLabel {
            id: [0; ID_CAP],
            label: [0; ID_CAP],
        };
        copy_text("Settings", &mut entry.id, id, "modelId")?;
        copy_text("Settings", &mut entry.label, label, "displayName")?;
        settings.model_label_count += 1;
    }
    Ok(())
}

fn read_model_id_array(
    array: Option<&Vec<Value>>,
    entries: &mut [RemovedModel],
    count: &mut usize,
    name: &str,
) -> Outcome<()> {
    let Some(array) = array else { return Ok(()) };
    if array.len() > MODEL_CAP {
        return Err(Failure::format(format!(
            "Settings field '{name}' must be a smaller array"
        )));
    }
    *count = 0;
    for item in array {
        let id = match item {
            Value::String(id) if !id.is_empty() && !id.bytes().any(|byte| byte == 0) => id,
            _ => {
                return Err(Failure::format(format!(
                    "Settings field '{name}' must be a non-empty string without embedded nulls"
                )))
            }
        };
        let entry = &mut entries[*count];
        *entry = RemovedModel { id: [0; ID_CAP] };
        copy_text("Settings", &mut entry.id, id, name)?;
        *count += 1;
    }
    Ok(())
}

fn read_extensions(value: Option<&Value>, settings: &mut Settings) -> Outcome<()> {
    let Some(value) = value else { return Ok(()) };
    let object = match value {
        Value::Object(object) => object,
        _ => {
            return Err(Failure::format(
                "Settings field 'extensions' must be an object".to_string(),
            ))
        }
    };
    let json = match serde_json::to_string(object) {
        Ok(json) => json,
        Err(_) => {
            return Err(Failure::memory(
                "Cannot preserve settings extensions".to_string(),
            ))
        }
    };
    if json.len() >= EXTENSIONS_CAP {
        return Err(Failure::format(format!(
            "Settings extensions exceed the {}-byte limit",
            EXTENSIONS_CAP - 1
        )));
    }
    settings.extensions_json.fill(0);
    for (index, byte) in json.bytes().enumerate() {
        settings.extensions_json[index] = byte as c_char;
    }
    Ok(())
}

fn gated_object(bytes: &[u8], format: &str) -> Outcome<Map<String, Value>> {
    let parsed: Value = match serde_json::from_slice::<UniqueValue>(bytes) {
        Ok(UniqueValue(parsed)) => parsed,
        Err(_) => return Err(Failure::format("Invalid configuration JSON".to_string())),
    };
    let object = match parsed {
        Value::Object(object) => object,
        _ => {
            return Err(Failure::format(format!(
                "Unsupported configuration format; expected {format} schema {SCHEMA_VERSION}"
            )))
        }
    };
    let matches = object.get("format") == Some(&Value::String(format.to_string()))
        && object.get("schemaVersion").and_then(|version| {
            version
                .as_i64()
                .or_else(|| version.as_u64().map(|v| v as i64))
        }) == Some(SCHEMA_VERSION);
    if !matches {
        return Err(Failure::format(format!(
            "Unsupported configuration format; expected {format} schema {SCHEMA_VERSION}"
        )));
    }
    Ok(object)
}

/// # Safety
/// `settings` must be writable for `settings_size` bytes and match the
/// bongo-safe `Settings` layout; `message` must be writable for
/// `message_capacity` bytes.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_settings_parse(
    bytes: *const u8,
    size: usize,
    settings: *mut Settings,
    settings_size: usize,
    message: *mut c_char,
    message_capacity: usize,
) -> c_int {
    crate::guard(LAYOUT, || {
        if bytes.is_null() && size > 0 || settings.is_null() || message.is_null() {
            return LAYOUT;
        }
        if settings_size != std::mem::size_of::<Settings>() {
            return LAYOUT;
        }
        let data = if size == 0 {
            &[][..]
        } else {
            std::slice::from_raw_parts(bytes, size)
        };
        match parse_settings(data, &mut *settings) {
            Ok(()) => OK,
            Err(failure) => write_message(message, message_capacity, &failure),
        }
    })
}

fn write_message(message: *mut c_char, message_capacity: usize, failure: &Failure) -> c_int {
    let out = unsafe { std::slice::from_raw_parts_mut(message.cast::<u8>(), message_capacity) };
    let bytes = failure.message.as_bytes();
    let copied = bytes.len().min(out.len().saturating_sub(1));
    out[..copied].copy_from_slice(&bytes[..copied]);
    out[copied] = 0;
    failure.code
}

fn parse_settings(bytes: &[u8], settings: &mut Settings) -> Outcome<()> {
    let root = gated_object(bytes, SETTINGS_FORMAT)?;
    let description = "Settings";
    let model = object(&root, description, "rendering")?;
    let window = object(&root, description, "window")?;
    let application = object(&root, description, "application")?;
    let shortcuts = object(&root, description, "shortcuts")?;
    let behaviors = array(&root, description, "behaviorOverrides")?;
    let random_disabled = array(&root, description, "randomBehaviorDisabled")?;
    let models = array(&root, description, "modelOverrides")?;
    let removed_models = array(&root, description, "removedModels")?;
    let hidden_models = array(&root, description, "hiddenModels")?;
    let model_order = array(&root, description, "modelOrder")?;
    let extensions = match root.get("extensions") {
        None | Some(Value::Null) => None,
        Some(value) => Some(value),
    };
    if let Some(model) = model {
        read_model(model, &mut settings.model)?;
    }
    if let Some(window) = window {
        read_window(window, &mut settings.window)?;
    }
    if let Some(application) = application {
        read_app(application, &mut settings.app)?;
    }
    if let Some(shortcuts) = shortcuts {
        read_shortcuts(shortcuts, &mut settings.shortcuts)?;
    }
    read_behaviors(behaviors, settings)?;
    read_random_disabled(random_disabled, settings)?;
    read_model_labels(models, settings)?;
    read_model_id_array(
        removed_models,
        &mut settings.removed_models,
        &mut settings.removed_model_count,
        "removedModels",
    )?;
    read_model_id_array(
        hidden_models,
        &mut settings.hidden_models,
        &mut settings.hidden_model_count,
        "hiddenModels",
    )?;
    read_model_id_array(
        model_order,
        &mut settings.model_order,
        &mut settings.model_order_count,
        "modelOrder",
    )?;
    read_extensions(extensions, settings)?;
    Ok(())
}

fn string_value(field: &[c_char]) -> Value {
    Value::String(c_string(field))
}

fn write_model(value: &ModelPreferences) -> Value {
    serde_json::json!({
        "multiplePets": value.multiple_pets,
        "modelMirrored": value.mirror,
        "modelFlippedVertically": value.vertical_flip,
        "pointerMirrored": value.mouse_mirror,
        "pointerFlippedVertically": value.mouse_vertical_flip,
        "centerPointerTracking": value.mouse_centered,
        "ignorePointerInput": value.ignore_mouse,
        "forceMouseInput": value.force_mouse_input,
        "gamepadFourHands": value.gamepad_four_hands,
        "dynamicTextureResolution": value.dynamic_texture_resolution,
        "renderQualityPercent": value.render_quality_percent,
        "maximumFps": value.max_fps,
    })
}

fn write_window(value: &WindowPreferences) -> Value {
    serde_json::json!({
        "clickThrough": value.pass_through,
        "alwaysOnTop": value.always_on_top,
        "hideOnPointerOver": value.hide_on_hover,
        "keepOnScreen": value.keep_in_screen,
        "edgeSnap": value.edge_snap,
        "captureOnly": value.capture_only,
        "captureBackground": value.obs_background,
        "customBackground": value.custom_background,
        "customBackgroundPath": string_value(&value.custom_background_path),
        "tightFrame": value.tight_frame,
        "randomExpression": value.random_expression,
        "randomMotion": value.random_motion,
        "sequentialModel": value.sequential_model,
        "roundedCorners": value.rounded_corners,
        "cornerRadiusPercent": value.corner_radius_percent,
        "captureBackgroundColor": format!("#{:06x}", value.obs_background_rgb & 0xffffff),
        "hideDelaySeconds": value.hide_delay_seconds,
        "hideFadeSeconds": value.hide_fade_seconds,
        "randomExpressionIntervalSeconds": value.random_expression_interval_seconds,
        "randomMotionIntervalSeconds": value.random_motion_interval_seconds,
        "sequentialModelIntervalSeconds": value.sequential_model_interval_seconds,
    })
}

fn write_app(value: &ApplicationPreferences) -> Value {
    let themes = ["auto", "light", "dark"];
    let languages = [
        "en-US", "zh-CN", "zh-Hant", "fr-FR", "de-DE", "ja-JP", "ko-KR", "pt-BR", "ru-RU", "es-ES",
    ];
    let render_backends = ["auto", "opengl", "vulkan", "metal"];
    serde_json::json!({
        "launchAtLogin": value.autostart,
        "runAsAdmin": value.run_as_admin,
        "showTrayIcon": value.tray_visible,
        "largeRenderOptimization": value.large_render_optimization,
        "largeRenderParallelRecording": value.render_parallel_recording,
        "largeRenderBindless": value.render_bindless,
        "largeRenderLargeAllocation": value.render_large_allocation,
        "theme": themes.get(value.theme as usize).unwrap_or(&themes[0]),
        "language": languages.get(value.language as usize).unwrap_or(&languages[0]),
        "renderBackend": render_backends.get(value.render_backend as usize).unwrap_or(&render_backends[0]),
    })
}

fn write_shortcuts(value: &ShortcutPreferences) -> Value {
    serde_json::json!({
        "toggleVisibility": c_string(&value.toggle_pet_visibility),
        "openSettings": c_string(&value.visible_preferences),
        "openMenu": c_string(&value.open_menu),
        "toggleModelMirror": c_string(&value.mirror),
        "toggleClickThrough": c_string(&value.pass_through),
        "toggleAlwaysOnTop": c_string(&value.always_on_top),
    })
}

fn write_behaviors(settings: &Settings) -> Value {
    let mut array = Vec::new();
    for value in &settings.behavior_shortcuts[..settings.behavior_shortcut_count] {
        if value.shortcut_external && value.label[0] == 0 {
            continue;
        }
        let mut item = Map::new();
        item.insert("behaviorId".into(), string_value(&value.id));
        if !value.shortcut_external && value.shortcut_disabled {
            item.insert("shortcutDisabled".into(), Value::Bool(true));
        }
        if !value.shortcut_external && value.shortcut[0] != 0 {
            item.insert("shortcut".into(), string_value(&value.shortcut));
        }
        if value.label[0] != 0 {
            item.insert("displayName".into(), string_value(&value.label));
        }
        array.push(Value::Object(item));
    }
    Value::Array(array)
}

fn write_random_disabled(settings: &Settings) -> Value {
    Value::Array(
        settings.random_disabled[..settings.random_disabled_count.min(RANDOM_DISABLED_CAP)]
            .iter()
            .map(|field| string_value(field))
            .collect(),
    )
}

fn write_model_labels(settings: &Settings) -> Value {
    Value::Array(
        settings.model_labels[..settings.model_label_count]
            .iter()
            .map(|entry| {
                serde_json::json!({
                    "modelId": c_string(&entry.id),
                    "displayName": c_string(&entry.label),
                })
            })
            .collect(),
    )
}

fn write_model_id_array(key: &str, entries: &[RemovedModel], count: usize) -> (String, Value) {
    (
        key.to_string(),
        Value::Array(
            entries[..count]
                .iter()
                .map(|entry| string_value(&entry.id))
                .collect(),
        ),
    )
}

fn write_extensions(settings: &Settings) -> Outcome<Value> {
    let json = c_string(&settings.extensions_json);
    let json = json.trim_end_matches('\0');
    let json = if json.is_empty() { "{}" } else { json };
    match serde_json::from_str::<Value>(json) {
        Ok(value @ Value::Object(_)) => Ok(value),
        _ => Err(Failure::format(
            "Settings extensions are not a valid JSON object".to_string(),
        )),
    }
}

fn build_settings_json(settings: &Settings) -> Outcome<Value> {
    let extensions = write_extensions(settings)?;
    let mut root = Map::new();
    root.insert("format".into(), Value::String(SETTINGS_FORMAT.into()));
    root.insert("schemaVersion".into(), Value::Number(SCHEMA_VERSION.into()));
    root.insert("rendering".into(), write_model(&settings.model));
    root.insert("window".into(), write_window(&settings.window));
    root.insert("application".into(), write_app(&settings.app));
    root.insert("shortcuts".into(), write_shortcuts(&settings.shortcuts));
    root.insert("behaviorOverrides".into(), write_behaviors(settings));
    root.insert(
        "randomBehaviorDisabled".into(),
        write_random_disabled(settings),
    );
    root.insert("modelOverrides".into(), write_model_labels(settings));
    let (key, value) = write_model_id_array(
        "removedModels",
        &settings.removed_models,
        settings.removed_model_count,
    );
    root.insert(key, value);
    let (key, value) = write_model_id_array(
        "hiddenModels",
        &settings.hidden_models,
        settings.hidden_model_count,
    );
    root.insert(key, value);
    let (key, value) = write_model_id_array(
        "modelOrder",
        &settings.model_order,
        settings.model_order_count,
    );
    root.insert(key, value);
    root.insert("extensions".into(), extensions);
    Ok(Value::Object(root))
}

/// # Safety
/// `settings` must be readable for `settings_size` bytes and match the
/// bongo-safe `Settings` layout; `json`/`length` must be writable. The
/// returned buffer is released with `bongo_safe_free_json`.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_settings_write(
    settings: *const Settings,
    settings_size: usize,
    json: *mut *mut u8,
    length: *mut usize,
) -> c_int {
    crate::guard(LAYOUT, || {
        if settings.is_null() || json.is_null() || length.is_null() {
            return LAYOUT;
        }
        *json = std::ptr::null_mut();
        *length = 0;
        if settings_size != std::mem::size_of::<Settings>() {
            return LAYOUT;
        }
        match build_settings_json(&*settings).and_then(|value| {
            serde_json::to_vec_pretty(&value)
                .map_err(|_| Failure::memory("Cannot serialize settings JSON".to_string()))
        }) {
            Ok(bytes) => {
                let mut bytes = bytes;
                let pointer = bytes.as_mut_ptr();
                *length = bytes.len();
                std::mem::forget(bytes);
                *json = pointer;
                OK
            }
            Err(failure) => failure.code,
        }
    })
}

/// # Safety
/// `session` must be writable for `session_size` bytes and match the
/// bongo-safe `SessionState` layout; `message` must be writable for
/// `message_capacity` bytes.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_session_parse(
    bytes: *const u8,
    size: usize,
    session: *mut SessionState,
    session_size: usize,
    message: *mut c_char,
    message_capacity: usize,
) -> c_int {
    crate::guard(LAYOUT, || {
        if bytes.is_null() && size > 0 || session.is_null() || message.is_null() {
            return LAYOUT;
        }
        if session_size != std::mem::size_of::<SessionState>() {
            return LAYOUT;
        }
        let data = if size == 0 {
            &[][..]
        } else {
            std::slice::from_raw_parts(bytes, size)
        };
        match parse_session(data, &mut *session) {
            Ok(()) => OK,
            Err(failure) => write_message(message, message_capacity, &failure),
        }
    })
}

fn parse_session(bytes: &[u8], session: &mut SessionState) -> Outcome<()> {
    let root = gated_object(bytes, SESSION_FORMAT)?;
    const DESCRIPTION: &str = "Session";
    if let Some(window) = object(&root, DESCRIPTION, "window")? {
        boolean(window, DESCRIPTION, "visible", &mut session.window.visible)?;
        real(
            window,
            DESCRIPTION,
            "scalePercent",
            &mut session.window.scale_percent,
        )?;
        real(
            window,
            DESCRIPTION,
            "opacityPercent",
            &mut session.window.opacity_percent,
        )?;
        if let Some(position) = object(window, DESCRIPTION, "position")? {
            integer(position, DESCRIPTION, "x", &mut session.window.x)?;
            integer(position, DESCRIPTION, "y", &mut session.window.y)?;
            session.window.position_known = true;
        }
        if let Some(size) = object(window, DESCRIPTION, "size")? {
            integer(size, DESCRIPTION, "width", &mut session.window.width)?;
            integer(size, DESCRIPTION, "height", &mut session.window.height)?;
            let mut content_width = 0;
            let mut content_height = 0;
            integer(size, DESCRIPTION, "contentWidth", &mut content_width)?;
            integer(size, DESCRIPTION, "contentHeight", &mut content_height)?;
            integer(
                size,
                DESCRIPTION,
                "contentLeft",
                &mut session.window.content_left,
            )?;
            integer(
                size,
                DESCRIPTION,
                "contentTop",
                &mut session.window.content_top,
            )?;
            session.window.content_width = if content_width > 0 {
                content_width
            } else {
                session.window.width
            };
            session.window.content_height = if content_height > 0 {
                content_height
            } else {
                session.window.height
            };
        }
    }
    if let Some(active_model) = root.get("activeModelId") {
        let text = match active_model {
            Value::String(text)
                if text.len() < ID_CAP
                    && !text.bytes().any(|byte| byte == 0) =>
            {
                text
            }
            _ => {
                return Err(Failure::format(
                    "Session field 'activeModelId' must be a string within the supported length".to_string(),
                ))
            }
        };
        session.active_model_id.fill(0);
        for (index, byte) in text.bytes().enumerate() {
            session.active_model_id[index] = byte as c_char;
        }
    }
    integer(
        &root,
        DESCRIPTION,
        "lastUpdateCheckDay",
        &mut session.last_update_check_day,
    )?;
    text(
        &root,
        DESCRIPTION,
        "lastUpdateCheckVersion",
        &mut session.last_update_check_version,
    )?;
    text(
        &root,
        DESCRIPTION,
        "availableUpdateVersion",
        &mut session.available_update_version,
    )?;
    if let Some(array) = array(&root, DESCRIPTION, "additionalModelIds")? {
        if array.len() > ADDITIONAL_MODEL_CAP {
            return Err(Failure::format(
                "Session field 'additionalModelIds' must be a smaller array".to_string(),
            ));
        }
        session.additional_model_count = 0;
        for item in array {
            match item {
                Value::String(id)
                    if !id.is_empty() && id.len() < ID_CAP && !id.bytes().any(|byte| byte == 0) =>
                {
                    let entry = &mut session.additional_model_ids[session.additional_model_count];
                    entry.fill(0);
                    for (index, byte) in id.bytes().enumerate() {
                        entry[index] = byte as c_char;
                    }
                    session.additional_model_count += 1;
                }
                _ => {
                    return Err(Failure::format(
                        "Session field 'additionalModelIds[]' must be a non-empty model id within the supported length".to_string(),
                    ))
                }
            }
        }
    }
    if let Some(array) = array(&root, DESCRIPTION, "activeBehaviors")? {
        if array.len() > BEHAVIOR_BINDING_CAP {
            return Err(Failure::format(
                "Session field 'activeBehaviors' must be a smaller array".to_string(),
            ));
        }
        session.active_behavior_count = 0;
        for item in array {
            let object = match item {
                Value::Object(object) => object,
                _ => {
                    return Err(Failure::format(
                        "Session field 'activeBehaviors[]' must be an object".to_string(),
                    ))
                }
            };
            let model_id = string(object, DESCRIPTION, "modelId")?;
            let behavior_id = string(object, DESCRIPTION, "behaviorId")?;
            let (model_id, behavior_id) = match (model_id, behavior_id) {
                (Some(model_id), Some(behavior_id))
                    if !model_id.is_empty() && !behavior_id.is_empty() =>
                {
                    (model_id, behavior_id)
                }
                _ => {
                    return Err(Failure::format(
                        "Session field 'activeBehaviors[]' must be non-empty modelId and behaviorId strings".to_string(),
                    ))
                }
            };
            let entry = &mut session.active_behaviors[session.active_behavior_count];
            *entry = ActiveBehavior {
                model_id: [0; ID_CAP],
                behavior_id: [0; BEHAVIOR_ID_CAP],
            };
            copy_text(DESCRIPTION, &mut entry.model_id, model_id, "modelId")?;
            copy_text(
                DESCRIPTION,
                &mut entry.behavior_id,
                behavior_id,
                "behaviorId",
            )?;
            session.active_behavior_count += 1;
        }
    }
    Ok(())
}

fn build_session_json(session: &SessionState) -> Outcome<Value> {
    let mut root = Map::new();
    root.insert("format".into(), Value::String(SESSION_FORMAT.into()));
    root.insert("schemaVersion".into(), Value::Number(SCHEMA_VERSION.into()));
    let window = &session.window;
    let mut window_object = Map::new();
    window_object.insert("visible".into(), Value::Bool(window.visible));
    window_object.insert("scalePercent".into(), Value::from(window.scale_percent));
    window_object.insert("opacityPercent".into(), Value::from(window.opacity_percent));
    if window.position_known {
        window_object.insert(
            "position".into(),
            serde_json::json!({"x": window.x, "y": window.y}),
        );
    }
    window_object.insert(
        "size".into(),
        serde_json::json!({
            "width": window.width,
            "height": window.height,
            "contentWidth": window.content_width,
            "contentHeight": window.content_height,
            "contentLeft": window.content_left,
            "contentTop": window.content_top,
        }),
    );
    root.insert("window".into(), Value::Object(window_object));
    root.insert(
        "activeModelId".into(),
        string_value(&session.active_model_id),
    );
    root.insert(
        "lastUpdateCheckDay".into(),
        Value::from(session.last_update_check_day),
    );
    root.insert(
        "lastUpdateCheckVersion".into(),
        string_value(&session.last_update_check_version),
    );
    root.insert(
        "availableUpdateVersion".into(),
        string_value(&session.available_update_version),
    );
    root.insert(
        "additionalModelIds".into(),
        Value::Array(
            session.additional_model_ids[..session.additional_model_count]
                .iter()
                .map(|field| string_value(field))
                .collect(),
        ),
    );
    root.insert(
        "activeBehaviors".into(),
        Value::Array(
            session.active_behaviors[..session.active_behavior_count]
                .iter()
                .map(|entry| {
                    serde_json::json!({
                        "modelId": c_string(&entry.model_id),
                        "behaviorId": c_string(&entry.behavior_id),
                    })
                })
                .collect(),
        ),
    );
    Ok(Value::Object(root))
}

/// # Safety
/// See `bongo_safe_settings_write`.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_session_write(
    session: *const SessionState,
    session_size: usize,
    json: *mut *mut u8,
    length: *mut usize,
) -> c_int {
    crate::guard(LAYOUT, || {
        if session.is_null() || json.is_null() || length.is_null() {
            return LAYOUT;
        }
        *json = std::ptr::null_mut();
        *length = 0;
        if session_size != std::mem::size_of::<SessionState>() {
            return LAYOUT;
        }
        match build_session_json(&*session).and_then(|value| {
            serde_json::to_vec_pretty(&value)
                .map_err(|_| Failure::memory("Cannot serialize session JSON".to_string()))
        }) {
            Ok(bytes) => {
                let mut bytes = bytes;
                let pointer = bytes.as_mut_ptr();
                *length = bytes.len();
                std::mem::forget(bytes);
                *json = pointer;
                OK
            }
            Err(failure) => failure.code,
        }
    })
}

/// # Safety
/// `json` must come from `bongo_safe_settings_write` or
/// `bongo_safe_session_write` with the same length.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_free_json(json: *mut u8, length: usize) {
    crate::guard((), || {
        if json.is_null() || length == 0 {
            return;
        }
        drop(crate::reclaim_exact(json, length));
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn metal_backend_survives_settings_serialization() {
        let app = ApplicationPreferences {
            autostart: false, run_as_admin: false, tray_visible: true,
            large_render_optimization: false,
            render_parallel_recording: true,
            render_bindless: true,
            render_large_allocation: true,
            theme: 0, language: 0, render_backend: 3,
        };
        let json = write_app(&app);
        assert_eq!(json["renderBackend"], "metal");
        assert_eq!(render_backend_from_name(json["renderBackend"].as_str().unwrap()), Some(3));
    }

    #[test]
    fn large_render_optimization_round_trips_and_rejects_invalid_types() {
        let mut app: ApplicationPreferences = unsafe { std::mem::zeroed() };
        assert!(read_app(&serde_json::json!({}).as_object().unwrap(), &mut app).is_ok());
        assert!(!app.large_render_optimization);
        assert!(read_app(&serde_json::json!({"largeRenderOptimization": true}).as_object().unwrap(), &mut app).is_ok());
        assert_eq!(write_app(&app)["largeRenderOptimization"], true);
        assert!(read_app(&serde_json::json!({"largeRenderOptimization": false}).as_object().unwrap(), &mut app).is_ok());
        assert!(!app.large_render_optimization);
        assert!(read_app(&serde_json::json!({"largeRenderOptimization": "true"}).as_object().unwrap(), &mut app).is_err());
        assert_eq!(std::mem::size_of::<ApplicationPreferences>(), 20);
    }

    #[test]
    fn render_optimization_children_migrate_and_round_trip_independently() {
        let mut app: ApplicationPreferences = unsafe { std::mem::zeroed() };
        read_app(serde_json::json!({"largeRenderOptimization": true}).as_object().unwrap(), &mut app).unwrap();
        assert!(app.render_parallel_recording && app.render_bindless);
        assert!(app.render_large_allocation);
        let keys = ["largeRenderParallelRecording", "largeRenderBindless", "largeRenderLargeAllocation"];
        for mask in 0..8 {
            let mut object = Map::new();
            object.insert("largeRenderOptimization".into(), Value::Bool(true));
            for (index, key) in keys.iter().enumerate() {
                object.insert((*key).into(), Value::Bool(mask & (1 << index) != 0));
            }
            read_app(&object, &mut app).unwrap();
            let json = write_app(&app);
            for key in keys {
                assert_eq!(json[key], object[key]);
            }
            // Switching off the parent retains all child preferences.
            app.large_render_optimization = false;
            let json = write_app(&app);
            for key in keys {
                assert_eq!(json[key], object[key]);
            }
        }
        for key in keys {
            let mut object = Map::new();
            object.insert(key.into(), Value::Null);
            read_app(&object, &mut app).unwrap();
            assert_eq!(write_app(&app)[key], true);
            for invalid in [serde_json::json!("true"), serde_json::json!(1), serde_json::json!([])] {
                let mut object = Map::new();
                object.insert(key.into(), invalid);
                assert!(read_app(&object, &mut app).is_err());
            }
        }
    }

    #[test]
    fn removed_async_compute_preference_is_ignored_and_not_written() {
        let mut app: ApplicationPreferences = unsafe { std::mem::zeroed() };
        let json = serde_json::json!({"largeRenderOptimization": true,
            "largeRenderAsyncCompute": true, "largeRenderBindless": false});
        read_app(json.as_object().unwrap(), &mut app).unwrap();
        assert!(app.large_render_optimization);
        assert!(!app.render_bindless);
        assert!(write_app(&app).get("largeRenderAsyncCompute").is_none());
    }

    #[test]
    fn empty_model_selection_round_trips() {
        let mut session: SessionState = unsafe { std::mem::zeroed() };
        assert!(parse_session(br#"{"format":"bongocat/session","schemaVersion":1,"activeModelId":""}"#, &mut session).is_ok());
        assert_eq!(session.active_model_id[0], 0);
        assert!(build_session_json(&session).map(|json| json["activeModelId"] == "").unwrap_or(false));
    }

    #[test]
    fn custom_background_round_trip_and_exclusion() {
        let mut window: WindowPreferences = unsafe { std::mem::zeroed() };
        let input = serde_json::json!({
            "captureBackground": true, "customBackground": true,
            "customBackgroundPath": "backgrounds/example.img"
        });
        assert!(read_window(input.as_object().unwrap(), &mut window).is_ok());
        assert!(window.custom_background && !window.obs_background);
        let saved = write_window(&window);
        assert_eq!(saved["customBackgroundPath"], input["customBackgroundPath"]);
        assert_eq!(saved["customBackground"], true);
        let empty = serde_json::json!({"customBackgroundPath": ""});
        assert!(read_window(empty.as_object().unwrap(), &mut window).is_ok());
        assert!(!window.custom_background);
        let invalid = serde_json::json!({"customBackgroundPath": 42});
        assert!(read_window(invalid.as_object().unwrap(), &mut window).is_err());
    }

    #[test]
    fn prints_layout_sizes() {
        println!(
            "RUST settings={} session={} model={} window={} app={} shortcuts={} behavior={} label={} state={}",
            std::mem::size_of::<Settings>(),
            std::mem::size_of::<SessionState>(),
            std::mem::size_of::<ModelPreferences>(),
            std::mem::size_of::<WindowPreferences>(),
            std::mem::size_of::<ApplicationPreferences>(),
            std::mem::size_of::<ShortcutPreferences>(),
            std::mem::size_of::<BehaviorShortcut>(),
            std::mem::size_of::<ModelLabel>(),
            std::mem::size_of::<WindowState>(),
        );
    }
}
