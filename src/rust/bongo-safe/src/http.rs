//! Shared cancellable HTTPS downloads. C owns UI/jobs, Rust owns network bytes.
use crate::{guard, reclaim_exact, release_exact};
use reqwest::{redirect, Client, Url};
use std::ffi::CStr;
use std::os::raw::{c_char, c_int, c_void};
use std::time::Duration;
use tokio::runtime::{Builder, Runtime};

pub(crate) const OK: c_int = 0;
pub(crate) const CANCELLED: c_int = 1;
const NETWORK: c_int = 2;
const RESPONSE: c_int = 3;
const MEMORY: c_int = 4;
const MAX_MEMORY_BODY: usize = 4 * 1024 * 1024;
const PROXY_PREFIX: &str = "https://gh-proxy.com/";

#[derive(Debug)]
pub(crate) struct Failure(pub c_int, pub String);
pub(crate) type Outcome<T> = Result<T, Failure>;
pub(crate) type Cancelled = Option<unsafe extern "C" fn(*mut c_void) -> bool>;
pub(crate) type Progress = Option<unsafe extern "C" fn(*mut c_void, u64, u64)>;

#[derive(Clone, Copy)]
pub(crate) struct Callbacks {
    pub cancelled: Cancelled,
    pub progress: Progress,
    pub userdata: *mut c_void,
}

impl Callbacks {
    pub fn check(self) -> Outcome<()> {
        if self
            .cancelled
            .is_some_and(|call| unsafe { call(self.userdata) })
        {
            Err(Failure(CANCELLED, "Request cancelled".into()))
        } else {
            Ok(())
        }
    }
    fn report(self, received: u64, total: u64) {
        if let Some(call) = self.progress {
            unsafe { call(self.userdata, received, total) };
        }
    }
}

pub struct BongoSafeHttpClient {
    runtime: Option<Runtime>,
    client: Client,
}

fn secure_url(url: &Url) -> bool {
    url.scheme() == "https"
        && url.host_str().is_some()
        && url.username().is_empty()
        && url.password().is_none()
        && url.port_or_known_default() == Some(443)
}

fn request_url(text: &str, accelerate: bool) -> Outcome<Url> {
    // Reject ambiguous input before the URL parser can normalize it.
    if text.bytes().any(|b| b <= 0x20 || b == b'\\') || text.len() > 4096 {
        return Err(Failure(RESPONSE, "Invalid HTTPS resource URL".into()));
    }
    let url = Url::parse(text).map_err(|_| Failure(RESPONSE, "Invalid resource URL".into()))?;
    if !secure_url(&url) {
        return Err(Failure(
            RESPONSE,
            "Only public HTTPS resources are supported".into(),
        ));
    }
    let github = matches!(
        url.host_str(),
        Some(
            "github.com"
                | "api.github.com"
                | "raw.githubusercontent.com"
                | "avatars.githubusercontent.com"
                | "objects.githubusercontent.com"
                | "release-assets.githubusercontent.com"
        )
    );
    if accelerate && github {
        Url::parse(&format!("{PROXY_PREFIX}{text}"))
            .map_err(|_| Failure(RESPONSE, "Invalid accelerated URL".into()))
    } else {
        Ok(url)
    }
}

fn network_error(error: reqwest::Error) -> Failure {
    let reason = if error.is_timeout() {
        "network timeout"
    } else if error.is_connect() {
        "DNS, connection or TLS error"
    } else {
        "network request failed"
    };
    Failure(NETWORK, format!("Network error: {reason}"))
}

impl BongoSafeHttpClient {
    pub(crate) fn new() -> Outcome<Self> {
        let runtime = Builder::new_current_thread()
            .enable_all()
            .build()
            .map_err(|_| Failure(MEMORY, "Cannot initialize the network runtime".into()))?;
        let client = Client::builder()
            .https_only(true)
            .connect_timeout(Duration::from_secs(5))
            .read_timeout(Duration::from_secs(15))
            .user_agent(concat!("BongoCat/", env!("CARGO_PKG_VERSION")))
            .referer(false)
            .redirect(redirect::Policy::custom(|attempt| {
                if attempt.previous().len() >= 5 || !secure_url(attempt.url()) {
                    attempt.error("Unsafe or excessive HTTPS redirect")
                } else {
                    attempt.follow()
                }
            }))
            .build()
            .map_err(network_error)?;
        Ok(Self {
            runtime: Some(runtime),
            client,
        })
    }

    pub(crate) fn fetch(
        &self,
        text: &str,
        accelerate: bool,
        limit: u64,
        timeout: Duration,
        callbacks: Callbacks,
        receive: impl FnMut(&[u8]) -> Outcome<()>,
    ) -> Outcome<()> {
        callbacks.check()?;
        let url = request_url(text, accelerate)?;
        let api = Url::parse(text)
            .ok()
            .is_some_and(|u| u.host_str() == Some("api.github.com"));
        self.fetch_url(url, api, limit, timeout, callbacks, receive)
    }

    fn fetch_url(
        &self,
        url: Url,
        api: bool,
        limit: u64,
        timeout: Duration,
        callbacks: Callbacks,
        mut receive: impl FnMut(&[u8]) -> Outcome<()>,
    ) -> Outcome<()> {
        let live2d = url.host_str() == Some("cubism.live2d.com") &&
            url.path().starts_with("/sdk-native/bin/");
        let mut request = self.client.get(url).timeout(timeout);
        if live2d {
            request = request.header("User-Agent", "Mozilla/5.0 (compatible; BongoCat)")
                .header("Referer", "https://www.live2d.com/en/sdk/download/native/");
        }
        if api {
            request = request
                .header("Accept", "application/vnd.github+json")
                .header("X-GitHub-Api-Version", "2022-11-28");
        }
        self.runtime.as_ref().unwrap().block_on(async {
            let transfer = async {
                let mut response = request.send().await.map_err(network_error)?;
                let status = response.status().as_u16();
                if status != 200 {
                    return Err(Failure(
                        RESPONSE,
                        format!("Server returned HTTP status {status}"),
                    ));
                }
                let total = response.content_length().unwrap_or(0);
                if total > limit {
                    return Err(Failure(RESPONSE, "Download exceeds the size limit".into()));
                }
                let mut received = 0u64;
                callbacks.report(0, total);
                while let Some(chunk) = response.chunk().await.map_err(network_error)? {
                    callbacks.check()?;
                    if chunk.len() as u64 > limit - received {
                        return Err(Failure(RESPONSE, "Download exceeds the size limit".into()));
                    }
                    receive(&chunk)?;
                    received += chunk.len() as u64;
                    callbacks.report(received, total);
                }
                if total != 0 && received != total {
                    return Err(Failure(RESPONSE, "Incomplete download".into()));
                }
                callbacks.check()
            };
            tokio::pin!(transfer);
            let mut tick = tokio::time::interval(Duration::from_millis(50));
            loop {
                tokio::select! {
                    result = &mut transfer => return result,
                    _ = tick.tick() => callbacks.check()?,
                }
            }
        })
    }

    pub(crate) fn get(
        &self,
        url: &str,
        accelerate: bool,
        limit: usize,
        callbacks: Callbacks,
    ) -> Outcome<Vec<u8>> {
        if limit == 0 || limit > MAX_MEMORY_BODY {
            return Err(Failure(RESPONSE, "Invalid response size limit".into()));
        }
        let mut data = Vec::new();
        self.fetch(
            url,
            accelerate,
            limit as u64,
            Duration::from_secs(15),
            callbacks,
            |chunk| {
                data.try_reserve(chunk.len())
                    .map_err(|_| Failure(MEMORY, "Cannot allocate response".into()))?;
                data.extend_from_slice(chunk);
                Ok(())
            },
        )?;
        Ok(data)
    }
}

impl Drop for BongoSafeHttpClient {
    fn drop(&mut self) {
        // System DNS can outlive a cancelled request; do not join it on shutdown.
        if let Some(runtime) = self.runtime.take() {
            runtime.shutdown_background();
        }
    }
}

pub(crate) unsafe fn text<'a>(value: *const c_char) -> Outcome<&'a str> {
    if value.is_null() {
        return Err(Failure(RESPONSE, "Missing argument".into()));
    }
    CStr::from_ptr(value)
        .to_str()
        .map_err(|_| Failure(RESPONSE, "Invalid UTF-8 argument".into()))
}

pub(crate) unsafe fn message(target: *mut c_char, capacity: usize, value: &str) {
    if target.is_null() || capacity == 0 {
        return;
    }
    let size = value.len().min(capacity - 1);
    std::ptr::copy_nonoverlapping(value.as_ptr(), target.cast(), size);
    *target.add(size) = 0;
}

#[no_mangle]
pub extern "C" fn bongo_safe_http_new() -> *mut BongoSafeHttpClient {
    guard(std::ptr::null_mut(), || {
        BongoSafeHttpClient::new()
            .map(|client| Box::into_raw(Box::new(client)))
            .unwrap_or(std::ptr::null_mut())
    })
}

/// Safety: the client must be unused and must have come from http_new.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_http_free(client: *mut BongoSafeHttpClient) {
    guard((), || {
        if !client.is_null() {
            drop(Box::from_raw(client));
        }
    });
}

/// Safety: strings/outputs are valid; callbacks and userdata live for this call.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_http_get(
    client: *mut BongoSafeHttpClient,
    url: *const c_char,
    accelerate: bool,
    limit: usize,
    cancelled: Cancelled,
    userdata: *mut c_void,
    response: *mut *mut u8,
    length: *mut usize,
    error: *mut c_char,
    capacity: usize,
) -> c_int {
    guard(MEMORY, || {
        if client.is_null() || response.is_null() || length.is_null() {
            return MEMORY;
        }
        *response = std::ptr::null_mut();
        *length = 0;
        message(error, capacity, "");
        let result = text(url).and_then(|url| {
            (*client).get(
                url,
                accelerate,
                limit,
                Callbacks {
                    cancelled,
                    progress: None,
                    userdata,
                },
            )
        });
        match result {
            Ok(mut bytes) => {
                *length = bytes.len();
                bytes.push(0);
                *response = release_exact(bytes);
                OK
            }
            Err(Failure(code, detail)) => {
                message(error, capacity, &detail);
                code
            }
        }
    })
}

/// Safety: response/length must match the returned allocation.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_free_http_response(response: *mut u8, length: usize) {
    guard((), || {
        drop(reclaim_exact(response, length.saturating_add(1)));
    });
}

#[cfg(test)]
#[path = "http_tests.rs"]
mod tests;
