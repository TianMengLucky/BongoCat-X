use crate::{Config, Host};
use bongo_plugin::io::{json, Value};
use bongo_plugin::page::text;
use std::ffi::{c_void, CString};
use std::sync::{
    atomic::{AtomicBool, Ordering},
    Arc,
};
unsafe extern "C" fn cancelled(context: *mut c_void) -> bool {
    context
        .cast::<AtomicBool>()
        .as_ref()
        .map_or(true, |c| c.load(Ordering::Acquire))
}
pub fn content(bytes: &[u8]) -> Result<String, u32> {
    if bytes.len() > 64 * 1024 {
        return Err(3);
    }
    let value: Value = serde_json::from_slice(bytes).map_err(|_| 3u32)?;
    let text = value
        .pointer("/choices/0/message/content")
        .and_then(Value::as_str)
        .ok_or(3u32)?;
    let text: String = text
        .chars()
        .filter(|c| !c.is_control() || *c == '\n')
        .take(512)
        .collect();
    let text = text.trim();
    if text.is_empty() {
        Err(3)
    } else {
        Ok(text.to_owned())
    }
}
pub fn request(
    host: Host,
    config: Config,
    index: usize,
    cancel: Arc<AtomicBool>,
) -> Result<String, u32> {
    let key = std::env::var(text(&config.key_env)).map_err(|_| 5u32)?;
    if key.is_empty() {
        return Err(5);
    }
    let events = [
        "The pet has started.",
        "The user has been idle.",
        "The user is typing.",
        "The user is using the mouse.",
        "The pet performed a behavior.",
    ];
    let mut body = json!({"model":text(&config.model),"stream":false,"max_tokens":512,
        "messages":[{"role":"system","content":text(&config.prompt)},
        {"role":"user","content":events[index]}]});
    if text(&config.endpoint).starts_with("https://api.deepseek.com/") {
        body["thinking"] = json!({"type":"disabled"});
    }
    let body = body.to_string();
    let url = CString::new(text(&config.endpoint)).map_err(|_| 3u32)?;
    let key = CString::new(key).map_err(|_| 5u32)?;
    let body = CString::new(body).map_err(|_| 3u32)?;
    let mut data = std::ptr::null_mut();
    let mut length = 0;
    let status = unsafe {
        (host.post)(
            url.as_ptr(),
            key.as_ptr(),
            body.as_ptr(),
            Some(cancelled),
            Arc::as_ptr(&cancel).cast_mut().cast(),
            &mut data,
            &mut length,
        )
    };
    if status != 0 {
        return Err(status as u32);
    }
    struct Response(Host, *mut u8, usize);
    impl Drop for Response {
        fn drop(&mut self) {
            unsafe { (self.0.free_response)(self.1, self.2) };
        }
    }
    let response = Response(host, data, length);
    if response.1.is_null() || response.2 > 64 * 1024 {
        return Err(3);
    }
    content(unsafe { std::slice::from_raw_parts(response.1, response.2) })
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn parses_only_assistant_content_and_bounds_unicode() {
        assert_eq!(
            content(br#"{"choices":[{"message":{"content":"hello"}}]}"#).unwrap(),
            "hello"
        );
        for bytes in [
            b"{}".as_slice(),
            b"{",
            br#"{"choices":[{"message":{"content":null}}]}"#,
            br#"{"choices":[{"message":{"content":"\u0000"}}]}"#,
        ] {
            assert!(content(bytes).is_err());
        }
        let value = json!({"choices":[{"message":{"content":"猫".repeat(1000)}}]});
        assert_eq!(
            content(value.to_string().as_bytes())
                .unwrap()
                .chars()
                .count(),
            512
        );
    }
}
