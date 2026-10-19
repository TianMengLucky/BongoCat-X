//! Bounded authenticated JSON POST, separate from the public download policy.
use crate::http::{self, Callbacks, Failure, Outcome};
use crate::{guard, release_exact};
use reqwest::{redirect, Client, Url};
use std::ffi::{c_char, c_int, c_void};
use std::time::Duration;

fn endpoint(text: &str) -> Outcome<Url> {
    let url = Url::parse(text).map_err(|_| Failure(3, "Invalid endpoint".into()))?;
    if text.len() > 2048
        || text.bytes().any(|b| b <= 0x20 || b == b'\\')
        || url.scheme() != "https"
        || url.host_str().is_none()
        || !url.username().is_empty()
        || url.password().is_some()
        || url.fragment().is_some()
    {
        return Err(Failure(3, "HTTPS endpoint required".into()));
    }
    Ok(url)
}
fn post(url: &str, key: &str, body: &str, callbacks: Callbacks) -> Outcome<Vec<u8>> {
    callbacks.check()?;
    let url = endpoint(url)?;
    if key.is_empty()
        || key.len() > 4096
        || key.bytes().any(|b| b < 0x21 || b == 0x7f)
        || body.len() > 64 * 1024
    {
        return Err(Failure(3, "Invalid request".into()));
    }
    let runtime = tokio::runtime::Builder::new_current_thread()
        .enable_all()
        .build()
        .map_err(|_| Failure(4, "Runtime unavailable".into()))?;
    let result = runtime.block_on(async {
        let request = async {
            let client = Client::builder()
                .https_only(true)
                .redirect(redirect::Policy::none())
                .connect_timeout(Duration::from_secs(5))
                .timeout(Duration::from_secs(30))
                .build()
                .map_err(|_| Failure(2, "Network setup failed".into()))?;
            let mut response = client
                .post(url)
                .bearer_auth(key)
                .header("Content-Type", "application/json")
                .body(body.to_owned())
                .send()
                .await
                .map_err(|_| Failure(2, "Request failed".into()))?;
            if !response.status().is_success() {
                // Never surface an error body or URL that may echo credentials.
                return Err(Failure(3, format!("HTTP {}", response.status().as_u16())));
            }
            let mut bytes = Vec::new();
            while let Some(chunk) = response
                .chunk()
                .await
                .map_err(|_| Failure(2, "Response failed".into()))?
            {
                if chunk.len() > 64 * 1024 - bytes.len() {
                    return Err(Failure(3, "Response too large".into()));
                }
                bytes.extend_from_slice(&chunk);
            }
            callbacks.check()?;
            Ok(bytes)
        };
        tokio::pin!(request);
        let mut tick = tokio::time::interval(Duration::from_millis(50));
        loop {
            tokio::select! {
                result = &mut request => break result,
                _ = tick.tick() => callbacks.check()?,
            }
        }
    });
    runtime.shutdown_background();
    result
}
/// # Safety
/// Arguments are NUL-terminated UTF-8; outputs and cancellation context remain
/// valid for this call. The caller releases successful bytes with free_http_response.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_http_post_json(
    url: *const c_char,
    key: *const c_char,
    body: *const c_char,
    cancelled: http::Cancelled,
    context: *mut c_void,
    response: *mut *mut u8,
    length: *mut usize,
) -> c_int {
    guard(4, || {
        if response.is_null() || length.is_null() {
            return 4;
        }
        *response = std::ptr::null_mut();
        *length = 0;
        let result = (|| {
            post(
                http::text(url)?,
                http::text(key)?,
                http::text(body)?,
                Callbacks {
                    cancelled,
                    progress: None,
                    userdata: context,
                },
            )
        })();
        match result {
            Ok(mut bytes) => {
                *length = bytes.len();
                bytes.push(0);
                *response = release_exact(bytes);
                0
            }
            Err(Failure(code, _)) => code,
        }
    })
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn endpoint_policy() {
        assert!(endpoint("https://api.deepseek.com/chat/completions").is_ok());
        assert!(endpoint("https://localhost:8443/v1/chat/completions").is_ok());
        for url in [
            "http://example.com",
            "https://key@example.com",
            "https://x/#secret",
            "file:///x",
        ] {
            assert!(endpoint(url).is_err());
        }
    }
}
