//! Package-to-runtime conversion is one operation; JSON nodes never cross per field.
use crate::http::{self, Failure};
use serde_json::{json, Value};
use std::{
    ffi::c_char,
    fs::{self, File},
    io::{Read, Write},
    path::{Path, PathBuf},
};
#[path = "import_chord.rs"]
mod chord;
#[path = "import_mver.rs"]
mod mver;
#[path = "import_render.rs"]
pub(crate) mod render;
#[path = "import_tauri.rs"]
mod tauri;
#[cfg(test)]
#[path = "import_tests.rs"]
mod tests;
type Outcome<T> = Result<T, Failure>;
#[repr(C)]
pub struct Metadata {
    config: *const c_char,
    directory: *const c_char,
    setting: *const c_char,
    assets: *const c_char,
    overrides: *const c_char,
    target: *const c_char,
    mode: *const c_char,
    format: i32,
    gamepad_buttons: bool,
}
pub(super) struct Package<'a> {
    config: &'a Path,
    directory: &'a Path,
    setting: &'a Path,
    assets: &'a Path,
    overrides: Option<&'a Path>,
    target: &'a Path,
    mode: &'a str,
    format: i32,
    gamepad_buttons: bool,
}
fn failure(message: &str) -> Failure {
    Failure(3, message.into())
}
fn io_error(error: std::io::Error) -> Failure {
    Failure(2, error.to_string())
}
pub(super) fn read(path: &Path) -> Outcome<String> {
    let mut bytes = Vec::new();
    File::open(path)
        .map_err(io_error)?
        .take(16 * 1024 * 1024 + 1)
        .read_to_end(&mut bytes)
        .map_err(io_error)?;
    if bytes.len() > 16 * 1024 * 1024 {
        return Err(failure("Model JSON exceeds 16 MiB"));
    }
    Ok(String::from_utf8_lossy(&bytes).into_owned())
}
pub(super) fn parse(text: &str) -> Outcome<Value> {
    crate::json_dom::read::compatible_json(text.trim_start_matches('\u{feff}'))
        .map_err(|(_, e)| Failure(3, e))
}
pub(super) fn write(path: &Path, value: &Value) -> Outcome<()> {
    let parent = path
        .parent()
        .ok_or_else(|| failure("Missing output directory"))?;
    let mut stage = tempfile::NamedTempFile::new_in(parent).map_err(io_error)?;
    serde_json::to_writer_pretty(&mut stage, value).map_err(|e| failure(&e.to_string()))?;
    stage.write_all(b"\n").map_err(io_error)?;
    stage.flush().map_err(io_error)?;
    stage.persist(path).map_err(|e| io_error(e.error))?;
    Ok(())
}
pub(super) fn dimensions(path: &Path) -> Option<(u32, u32)> {
    image::ImageReader::open(path)
        .ok()?
        .with_guessed_format()
        .ok()?
        .into_dimensions()
        .ok()
}
pub(super) fn asset(package: &Package<'_>, name: &Path) -> Option<PathBuf> {
    package
        .overrides
        .map(|root| root.join(name))
        .filter(|path| path.is_file())
        .or_else(|| Some(package.assets.join(name)).filter(|path| path.is_file()))
}
fn convert(package: &Package<'_>) -> Outcome<Value> {
    let format = match package.format {
        0 => "tauri-live2d",
        1 => "bongo-cat-mver",
        2 => "bongo-cat-mver-patch",
        3 => "inochi2d",
        _ => return Err(failure("Unknown import format")),
    };
    let mut result = json!({"schemaVersion":1,"kind":"bongo-cat-runtime-adapter","sourceFormat":format,"bindings":[]});
    if matches!(package.format, 1 | 2) {
        mver::convert(package, &mut result)?;
    } else {
        result["render"] = json!({"profile":"native"});
    }
    Ok(result)
}
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_import_metadata(
    raw: *const Metadata,
    size: usize,
    error: *mut c_char,
    error_capacity: usize,
) -> bool {
    crate::guard(false, || {
        let result = (|| {
            if size != std::mem::size_of::<Metadata>() {
                return Err(failure("Import ABI layout mismatch"));
            }
            let raw = raw
                .as_ref()
                .ok_or_else(|| failure("Missing import metadata"))?;
            let overrides = http::text(raw.overrides)?;
            let package = Package {
                config: Path::new(http::text(raw.config)?),
                directory: Path::new(http::text(raw.directory)?),
                setting: Path::new(http::text(raw.setting)?),
                assets: Path::new(http::text(raw.assets)?),
                overrides: (!overrides.is_empty()).then(|| Path::new(overrides)),
                target: Path::new(http::text(raw.target)?),
                mode: http::text(raw.mode)?,
                format: raw.format,
                gamepad_buttons: raw.gamepad_buttons,
            };
            write(
                &package.target.join(".bongo-cat-adapter.json"),
                &convert(&package)?,
            )
        })();
        if let Err(Failure(_, message)) = &result {
            http::message(error, error_capacity, message);
        }
        result.is_ok()
    })
}
