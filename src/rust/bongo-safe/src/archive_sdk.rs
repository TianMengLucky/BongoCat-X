use super::*;
fn compatible(name: &str) -> bool {
    let name = name.to_ascii_lowercase();
    if [
        "ios",
        "catalyst",
        "android",
        "harmonyos",
        "uwp",
        "emscripten",
    ]
    .iter()
    .any(|v| name.contains(v))
    {
        return false;
    }
    if cfg!(windows) {
        let x64 = name.contains("x86_64");
        let x86 = name.contains("x86") && !x64;
        name.ends_with("live2dcubismcore.dll")
            && !name.contains("arm")
            && !(x64 && cfg!(target_pointer_width = "32"))
            && !(x86 && cfg!(target_pointer_width = "64"))
    } else if cfg!(target_os = "macos") {
        name.ends_with("liblive2dcubismcore.dylib") && name.contains("macos")
    } else {
        name.ends_with("liblive2dcubismcore.so")
            && !name.contains("rpi")
            && if cfg!(any(target_arch = "arm", target_arch = "aarch64")) {
                name.contains("arm") || name.contains("aarch64")
            } else {
                name.contains("x86_64") && !name.contains("arm") && !name.contains("aarch64")
            }
    }
}
fn common_root(entries: &[Entry]) -> Option<PathBuf> {
    let mut files = entries.iter().filter(|e| !e.directory);
    let first = files.next()?;
    if first.name.components().count() < 2 {
        return None;
    }
    let root = PathBuf::from(first.name.components().next()?.as_os_str());
    entries
        .iter()
        .all(|e| e.name.starts_with(&root))
        .then_some(root)
}
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_sdk_extract(
    source: *const c_char,
    target: *const c_char,
    core: *mut c_char,
    capacity: usize,
) -> bool {
    if core.is_null() || capacity == 0 {
        return false;
    }
    *core = 0;
    crate::guard(false, || {
        let result = (|| {
            let source = Path::new(http::text(source)?);
            let target = Path::new(http::text(target)?);
            let mut zip = ZipArchive::new(File::open(source).map_err(io_error)?)
                .map_err(|_| format_error())?;
            let entries = plan(&mut zip)?;
            let strip = common_root(&entries);
            let chosen = entries
                .iter()
                .find(|e| {
                    !e.directory && e.size >= 64 * 1024 && compatible(&e.name.to_string_lossy())
                })
                .ok_or_else(format_error)?;
            let name = if let Some(strip) = &strip {
                chosen
                    .name
                    .strip_prefix(strip)
                    .map_err(|_| format_error())?
            } else {
                &chosen.name
            };
            let destination = target.join(name);
            // Reuse only a regular Core reached through directories, never symlinks.
            let mut cursor = target.to_path_buf();
            if fs::symlink_metadata(&cursor).is_ok_and(|m| m.file_type().is_symlink()) {
                return Err(format_error());
            }
            for component in name.components() {
                cursor.push(component);
                if fs::symlink_metadata(&cursor).is_ok_and(|m| m.file_type().is_symlink()) {
                    return Err(format_error());
                }
            }
            if destination.is_file() {
                output(&destination, core, capacity)?;
                return Ok(());
            }
            let parent = target.parent().ok_or_else(format_error)?;
            fs::create_dir_all(parent).map_err(io_error)?;
            let stage = tempfile::Builder::new()
                .prefix("sdk-")
                .tempdir_in(parent)
                .map_err(io_error)?;
            unpack(&mut zip, &entries, stage.path(), strip.as_deref())?;
            // Publishing replaces only an empty destination created by the host.
            if target.exists() {
                fs::remove_dir(target).map_err(io_error)?;
            }
            output(&destination, core, capacity)?;
            fs::rename(stage.path(), target).map_err(io_error)?;
            Ok::<_, Failure>(())
        })();
        if result.is_err() {
            *core = 0;
        }
        result.is_ok()
    })
}
