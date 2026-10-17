//! Bounded store downloads using the shared HTTPS client and SHA-256 verifier.
use crate::{
    guard,
    http::{self, BongoSafeHttpClient, Callbacks, Failure},
    update::verify_file,
};
use std::{
    io::Write,
    os::raw::{c_char, c_int, c_void},
    path::Path,
    time::Duration,
};
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_store_download(
    url: *const c_char,
    hash: *const c_char,
    target: *const c_char,
    accelerate: bool,
    cancelled: http::Cancelled,
    progress: http::Progress,
    userdata: *mut c_void,
    error: *mut c_char,
    capacity: usize,
) -> c_int {
    guard(4, || {
        let result = (|| {
            let url = http::text(url)?;
            let parsed = reqwest::Url::parse(url)
                .map_err(|_| Failure(3, "Invalid model download URL".into()))?;
            if parsed.scheme() != "https" || parsed.host_str() != Some("raw.githubusercontent.com")
                || !parsed.username().is_empty() || parsed.password().is_some()
                || parsed.port().is_some() || parsed.query().is_some() || parsed.fragment().is_some() {
                return Err(Failure(3, "Untrusted model download URL".into()));
            }
            let hash = http::text(hash)?;
            if hash.len() != 64 || !hash.bytes().all(|b| b.is_ascii_hexdigit()) {
                return Err(Failure(3, "Missing model SHA-256".into()));
            }
            let mut digest = [0u8; 32];
            for (i, byte) in digest.iter_mut().enumerate() {
                *byte = u8::from_str_radix(&hash[i * 2..i * 2 + 2], 16)
                    .map_err(|_| Failure(3, "Invalid SHA-256".into()))?;
            }
            let target = Path::new(http::text(target)?);
            let parent = target
                .parent()
                .ok_or_else(|| Failure(3, "Missing cache directory".into()))?;
            std::fs::create_dir_all(parent).map_err(crate::update_archive::io_error)?;
            let mut file =
                tempfile::NamedTempFile::new_in(parent).map_err(crate::update_archive::io_error)?;
            let callbacks = Callbacks {
                cancelled,
                progress,
                userdata,
            };
            BongoSafeHttpClient::new()?.fetch(
                url,
                accelerate,
                512 * 1024 * 1024,
                Duration::from_secs(1800),
                callbacks,
                |chunk| {
                    file.write_all(chunk)
                        .map_err(crate::update_archive::io_error)
                },
            )?;
            file.as_file()
                .sync_all()
                .map_err(crate::update_archive::io_error)?;
            verify_file(file.path(), &digest, callbacks).map_err(|failure| {
                Failure(
                    failure.0,
                    failure.1.replace("Update package", "Model package"),
                )
            })?;
            callbacks.check()?;
            file.persist(target)
                .map_err(|e| crate::update_archive::io_error(e.error))?;
            Ok::<(), Failure>(())
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
    fn verifies_exact_downloaded_bytes_and_honors_cancellation() {
        let mut file = tempfile::NamedTempFile::new().unwrap();
        file.write_all(b"abc").unwrap();
        file.flush().unwrap();
        let digest = [
            0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde, 0x5d, 0xae,
            0x22, 0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61,
            0xf2, 0x00, 0x15, 0xad,
        ];
        let callbacks = Callbacks {
            cancelled: None,
            progress: None,
            userdata: std::ptr::null_mut(),
        };
        assert!(verify_file(file.path(), &digest, callbacks).is_ok());
        file.write_all(b"corruption").unwrap();
        file.flush().unwrap();
        assert!(verify_file(file.path(), &digest, callbacks).is_err());
        unsafe extern "C" fn cancelled(_: *mut c_void) -> bool {
            true
        }
        let failure = verify_file(
            file.path(),
            &digest,
            Callbacks {
                cancelled: Some(cancelled),
                ..callbacks
            },
        )
        .unwrap_err();
        assert_eq!(failure.0, http::CANCELLED);
    }
}
