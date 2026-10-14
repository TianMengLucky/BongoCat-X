//! Official SDK download; only the matching dynamic Core is extracted at runtime.
use crate::{
    guard,
    http::{self, BongoSafeHttpClient, Callbacks, Failure},
};
use std::{
    io::{Read, Write},
    os::raw::{c_char, c_int, c_void},
    path::Path,
    time::Duration,
};
const SDK: &str = "https://cubism.live2d.com/sdk-native/bin/CubismSdkForNative-5-r.5.zip";
const CORE_LIMIT: u64 = 64 * 1024 * 1024;
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_live2d_license_text(
    bytes: *const u8,
    length: usize,
    output: *mut c_char,
    capacity: usize,
) -> bool {
    guard(false, || {
        if bytes.is_null()
            || length == 0
            || length > 1024 * 1024
            || output.is_null()
            || capacity == 0
        {
            return false;
        }
        let data = std::slice::from_raw_parts(bytes, length);
        if std::str::from_utf8(data).is_err() {
            return false;
        }
        let Ok(text) = html2text::from_read(data, 72) else {
            return false;
        };
        if text.is_empty() || text.len() >= capacity || text.contains('\0') {
            return false;
        }
        std::ptr::copy_nonoverlapping(text.as_ptr(), output.cast(), text.len());
        *output.add(text.len()) = 0;
        true
    })
}
fn extract(archive: &Path, target: &Path, x64: bool, callbacks: Callbacks) -> Result<(), Failure> {
    let mut zip = zip::ZipArchive::new(
        std::fs::File::open(archive).map_err(crate::update_archive::io_error)?,
    )
    .map_err(|_| Failure(3, "Invalid Live2D SDK archive".into()))?;
    if zip.len() > 20000 {
        return Err(Failure(3, "SDK archive has too many entries".into()));
    }
    let expected = format!(
        "CubismSdkForNative-5-r.5/Core/dll/windows/{}/Live2DCubismCore.dll",
        if x64 { "x86_64" } else { "x86" }
    );
    let mut core = zip
        .by_name(&expected)
        .map_err(|_| Failure(3, "SDK has no compatible dynamic Cubism Core".into()))?;
    if core.size() == 0 || core.size() > CORE_LIMIT {
        return Err(Failure(3, "Cubism Core exceeds the size limit".into()));
    }
    let parent = target
        .parent()
        .ok_or_else(|| Failure(3, "Missing Core directory".into()))?;
    let mut file =
        tempfile::NamedTempFile::new_in(parent).map_err(crate::update_archive::io_error)?;
    let mut buffer = [0u8; 65536];
    let mut size = 0u64;
    loop {
        callbacks.check()?;
        let count = core
            .read(&mut buffer)
            .map_err(crate::update_archive::io_error)?;
        if count == 0 {
            break;
        }
        if count as u64 > CORE_LIMIT - size {
            return Err(Failure(3, "Cubism Core exceeds the size limit".into()));
        }
        if size == 0 && (count < 2 || &buffer[..2] != b"MZ") {
            return Err(Failure(3, "SDK contains an invalid Core DLL".into()));
        }
        file.write_all(&buffer[..count])
            .map_err(crate::update_archive::io_error)?;
        size += count as u64;
    }
    if size < 2 {
        return Err(Failure(3, "SDK contains an empty Core DLL".into()));
    }
    file.as_file()
        .sync_all()
        .map_err(crate::update_archive::io_error)?;
    callbacks.check()?;
    file.persist(target)
        .map_err(|e| crate::update_archive::io_error(e.error))?;
    Ok(())
}
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_live2d_download(
    target: *const c_char,
    x64: bool,
    cancelled: http::Cancelled,
    progress: http::Progress,
    userdata: *mut c_void,
    error: *mut c_char,
    capacity: usize,
) -> c_int {
    guard(4, || {
        let result = (|| {
            let target = Path::new(http::text(target)?);
            let parent = target
                .parent()
                .ok_or_else(|| Failure(3, "Missing Core directory".into()))?;
            std::fs::create_dir_all(parent).map_err(crate::update_archive::io_error)?;
            let mut archive =
                tempfile::NamedTempFile::new_in(parent).map_err(crate::update_archive::io_error)?;
            let callbacks = Callbacks {
                cancelled,
                progress,
                userdata,
            };
            BongoSafeHttpClient::new()?.fetch(
                SDK,
                false,
                512 * 1024 * 1024,
                Duration::from_secs(1800),
                callbacks,
                |bytes| {
                    archive
                        .write_all(bytes)
                        .map_err(crate::update_archive::io_error)
                },
            )?;
            archive.flush().map_err(crate::update_archive::io_error)?;
            extract(archive.path(), target, x64, callbacks)
        })();
        match result {
            Ok(()) => {
                http::message(error, capacity, "");
                http::OK
            }
            Err(Failure(code, message)) => {
                http::message(error, capacity, &message);
                code
            }
        }
    })
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn license_html_preserves_text_and_rejects_truncation() {
        let input = b"<h1>License</h1><p>Read all terms.</p><script>hidden()</script>";
        let mut output = [0i8; 256];
        assert!(unsafe {
            bongo_safe_live2d_license_text(
                input.as_ptr(),
                input.len(),
                output.as_mut_ptr(),
                output.len(),
            )
        });
        let text = unsafe { std::ffi::CStr::from_ptr(output.as_ptr()) }
            .to_str()
            .unwrap();
        assert!(text.contains("License") && text.contains("Read all terms."));
        assert!(!text.contains("hidden()"));
        assert!(!unsafe {
            bongo_safe_live2d_license_text(input.as_ptr(), input.len(), output.as_mut_ptr(), 8)
        });
    }
    #[test]
    fn extraction_uses_only_matching_core_and_preserves_target_on_error() {
        let directory = tempfile::tempdir().unwrap();
        let archive = directory.path().join("sdk.zip");
        let target = directory.path().join("Core.dll");
        std::fs::write(&target, b"previous").unwrap();
        let mut writer = zip::ZipWriter::new(std::fs::File::create(&archive).unwrap());
        let options = zip::write::SimpleFileOptions::default();
        writer.start_file("../escape.dll", options).unwrap();
        writer.write_all(b"MZescape").unwrap();
        writer
            .start_file(
                "CubismSdkForNative-5-r.5/Core/dll/windows/x86_64/Live2DCubismCore.dll",
                options,
            )
            .unwrap();
        writer.write_all(b"MZtest-core").unwrap();
        writer.finish().unwrap();
        let callbacks = Callbacks {
            cancelled: None,
            progress: None,
            userdata: std::ptr::null_mut(),
        };
        assert!(extract(&archive, &target, false, callbacks).is_err());
        assert_eq!(std::fs::read(&target).unwrap(), b"previous");
        unsafe extern "C" fn cancel(_: *mut c_void) -> bool {
            true
        }
        let stopped = extract(
            &archive,
            &target,
            true,
            Callbacks {
                cancelled: Some(cancel),
                ..callbacks
            },
        )
        .unwrap_err();
        assert_eq!(stopped.0, http::CANCELLED);
        assert_eq!(std::fs::read(&target).unwrap(), b"previous");
        assert!(extract(&archive, &target, true, callbacks).is_ok());
        assert_eq!(std::fs::read(&target).unwrap(), b"MZtest-core");
        assert!(!directory
            .path()
            .parent()
            .unwrap()
            .join("escape.dll")
            .exists());
    }
}
