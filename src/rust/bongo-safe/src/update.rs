//! Release download, checksum verification and deferred update preparation.
use crate::guard;
use crate::http::{self, BongoSafeHttpClient, Callbacks, Cancelled, Failure, Outcome, Progress};
use crate::update_archive::{self, io_error};
use sha2::{Digest, Sha256};
use std::fs::{self, File};
use std::io::{Read, Write};
use std::os::raw::{c_char, c_int, c_void};
use std::path::{Path, PathBuf};
use std::time::Duration;
use tempfile::TempDir;

const DOWNLOAD_PREFIX: &str = "https://github.com/TianMengLucky/BongoCat-X/releases/download/";
const PACKAGE_LIMIT: u64 = 512 * 1024 * 1024;

pub struct BongoSafeUpdate {
    directory: Option<TempDir>,
    pub(crate) stage: PathBuf,
    pub(crate) package: PathBuf,
    pub(crate) source: PathBuf,
    pub(crate) target: PathBuf,
    pub(crate) executable: PathBuf,
    pub(crate) installed: bool,
    digest: [u8; 32],
}

fn checksum(bytes: &[u8], filename: &str) -> Outcome<[u8; 32]> {
    let text =
        std::str::from_utf8(bytes).map_err(|_| Failure(3, "Invalid release checksum".into()))?;
    let mut parts = text.trim_start_matches('\u{feff}').split_whitespace();
    let hash = parts.next().unwrap_or("");
    let name = parts.next().unwrap_or("").trim_start_matches('*');
    if hash.len() != 64
        || !hash.bytes().all(|b| b.is_ascii_hexdigit())
        || name != filename
        || parts.next().is_some()
    {
        return Err(Failure(
            3,
            "Invalid release checksum or package name".into(),
        ));
    }
    let mut result = [0u8; 32];
    for (index, byte) in result.iter_mut().enumerate() {
        *byte = u8::from_str_radix(&hash[index * 2..index * 2 + 2], 16)
            .map_err(|_| Failure(3, "Invalid release checksum".into()))?;
    }
    Ok(result)
}

pub(crate) fn verify_file(path: &Path, expected: &[u8; 32], callbacks: Callbacks) -> Outcome<()> {
    let mut file = File::open(path).map_err(io_error)?;
    let mut digest = Sha256::new();
    let mut chunk = [0u8; 64 * 1024];
    loop {
        callbacks.check()?;
        let count = file.read(&mut chunk).map_err(io_error)?;
        if count == 0 {
            break;
        }
        digest.update(&chunk[..count]);
    }
    if digest.finalize().as_slice() != expected {
        return Err(Failure(3, "Update package SHA-256 mismatch".into()));
    }
    Ok(())
}

fn prepare(
    url: &str,
    accelerate: bool,
    cache: &Path,
    executable: &Path,
    installed: bool,
    callbacks: Callbacks,
) -> Outcome<BongoSafeUpdate> {
    let resource = url
        .strip_prefix(DOWNLOAD_PREFIX)
        .ok_or_else(|| Failure(3, "Untrusted update package URL".into()))?;
    let (tag, filename) = resource
        .split_once('/')
        .ok_or_else(|| Failure(3, "Invalid update package URL".into()))?;
    if tag.is_empty()
        || filename.is_empty()
        || filename.contains(['/', '\\', '?', '#', ':', '%'])
        || !filename.starts_with("BongoCat-")
    {
        return Err(Failure(3, "Invalid update package name".into()));
    }
    fs::create_dir_all(cache).map_err(io_error)?;
    let directory = tempfile::Builder::new()
        .prefix("bongo-update-")
        .tempdir_in(cache)
        .map_err(io_error)?;
    let stage = fs::canonicalize(directory.path()).map_err(io_error)?;
    let package = stage.join(filename);
    let client = BongoSafeHttpClient::new()?;
    let sidecar = client.get(
        &format!("{url}.sha256"),
        accelerate,
        4096,
        Callbacks {
            progress: None,
            ..callbacks
        },
    )?;
    let expected = checksum(&sidecar, filename)?;
    let mut output = File::create_new(&package).map_err(io_error)?;
    client.fetch(
        url,
        accelerate,
        PACKAGE_LIMIT,
        Duration::from_secs(1800),
        callbacks,
        |chunk| output.write_all(chunk).map_err(io_error),
    )?;
    output.sync_all().map_err(io_error)?;
    drop(output);
    verify_file(&package, &expected, callbacks)?;
    let executable = fs::canonicalize(executable).map_err(io_error)?;
    let mut target = executable
        .parent()
        .ok_or_else(|| Failure(3, "Missing application directory".into()))?
        .to_owned();
    let source = if installed && cfg!(windows) && filename.ends_with("-setup.exe") {
        stage.clone()
    } else if filename.ends_with(".AppImage") && cfg!(target_os = "linux") {
        let appimage = std::env::var_os("APPIMAGE")
            .ok_or_else(|| Failure(3, "AppImage update requires an AppImage launch".into()))?;
        let appimage = fs::canonicalize(appimage).map_err(io_error)?;
        target = appimage.parent().unwrap().to_owned();
        let source = stage.join("payload");
        fs::create_dir(&source).map_err(io_error)?;
        let destination = source.join(appimage.file_name().unwrap());
        fs::copy(&package, &destination).map_err(io_error)?;
        #[cfg(unix)]
        {
            use std::os::unix::fs::PermissionsExt;
            fs::set_permissions(&destination, fs::Permissions::from_mode(0o755))
                .map_err(io_error)?;
        }
        return finish(
            directory, package, source, target, appimage, false, expected,
        );
    } else {
        let zip_format = filename.ends_with(".zip");
        if !zip_format && !filename.ends_with(".tar.gz") {
            return Err(Failure(3, "Unsupported update package format".into()));
        }
        let extracted = stage.join("payload");
        update_archive::extract(&package, &extracted, zip_format, callbacks)?;
        let (source, application) = update_archive::layout(&extracted, &executable)?;
        target = application;
        source
    };
    finish(
        directory, package, source, target, executable, installed, expected,
    )
}

fn finish(
    directory: TempDir,
    package: PathBuf,
    source: PathBuf,
    target: PathBuf,
    executable: PathBuf,
    installed: bool,
    digest: [u8; 32],
) -> Outcome<BongoSafeUpdate> {
    if !installed {
        update_archive::validate_target(&source, &target)?;
    }
    let stage = fs::canonicalize(directory.path()).map_err(io_error)?;
    Ok(BongoSafeUpdate {
        directory: Some(directory),
        stage,
        package,
        source,
        target,
        executable,
        installed,
        digest,
    })
}

/// Safety: valid UTF-8 inputs, writable outputs and live callback userdata.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_update_prepare(
    url: *const c_char,
    accelerate: bool,
    cache: *const c_char,
    executable: *const c_char,
    installed: bool,
    cancelled: Cancelled,
    progress: Progress,
    userdata: *mut c_void,
    output: *mut *mut BongoSafeUpdate,
    error: *mut c_char,
    capacity: usize,
) -> c_int {
    guard(4, || {
        if output.is_null() {
            return 4;
        }
        *output = std::ptr::null_mut();
        http::message(error, capacity, "");
        let result = (|| {
            prepare(
                http::text(url)?,
                accelerate,
                Path::new(http::text(cache)?),
                Path::new(http::text(executable)?),
                installed,
                Callbacks {
                    cancelled,
                    progress,
                    userdata,
                },
            )
        })();
        match result {
            Ok(update) => {
                *output = Box::into_raw(Box::new(update));
                http::OK
            }
            Err(Failure(code, detail)) => {
                http::message(error, capacity, &detail);
                code
            }
        }
    })
}

/// Safety: update is an unused record from update_prepare; inputs/outputs valid.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_update_launch(
    update: *mut BongoSafeUpdate,
    nearby_root: *const c_char,
    error: *mut c_char,
    capacity: usize,
) -> bool {
    guard(false, || {
        if update.is_null() {
            return false;
        }
        let update = &mut *update;
        let result = (|| {
            if update.directory.is_none() {
                return Err(Failure(3, "Update already launched".into()));
            }
            verify_file(
                &update.package,
                &update.digest,
                Callbacks {
                    cancelled: None,
                    progress: None,
                    userdata: std::ptr::null_mut(),
                },
            )?;
            if !update.installed {
                update_archive::validate_target(&update.source, &update.target)?;
            }
            crate::update_apply::launch(update, http::text(nearby_root)?)?;
            if let Some(directory) = update.directory.take() {
                let _ = directory.keep();
            }
            Ok(())
        })();
        match result {
            Ok(()) => true,
            Err(Failure(_, detail)) => {
                http::message(error, capacity, &detail);
                false
            }
        }
    })
}

/// Safety: update must not be freed twice or used by a concurrent request.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_update_free(update: *mut BongoSafeUpdate) {
    guard((), || {
        if !update.is_null() {
            drop(Box::from_raw(update));
        }
    });
}

#[cfg(test)]
#[path = "update_tests.rs"]
mod tests;
