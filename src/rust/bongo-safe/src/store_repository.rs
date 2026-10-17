//! Persist a repository without changing the public settings or plugin ABI.
use crate::config_ui::read;
use reqwest::Url;
use serde_json::Value;
use std::ffi::{c_char, CStr};

const DEFAULT: &str = "https://github.com/TianMengLucky/Awesome-BongoCat";

fn canonical(text: &str) -> Option<String> {
    let text = text.trim().trim_end_matches('/');
    let url = Url::parse(text).ok()?;
    let path = text.strip_prefix("https://github.com/")?;
    let parts: Vec<_> = path.split('/').collect();
    if url.scheme() != "https"
        || url.host_str() != Some("github.com")
        || !url.username().is_empty()
        || url.password().is_some()
        || url.port().is_some()
        || url.query().is_some()
        || url.fragment().is_some()
        || parts.len() != 2
        || text.len() >= 512
        || parts.iter().any(|part| {
            part.is_empty()
                || *part == "."
                || *part == ".."
                || !part
                    .bytes()
                    .all(|b| b.is_ascii_alphanumeric() || b"-_.".contains(&b))
        })
    {
        return None;
    }
    let repo = parts[1].strip_suffix(".git").unwrap_or(parts[1]);
    if repo.is_empty() {
        return None;
    }
    Some(format!("https://github.com/{}/{repo}", parts[0]))
}
unsafe fn copy(value: &str, out: *mut c_char, capacity: usize) -> bool {
    if out.is_null() || value.len() >= capacity {
        return false;
    }
    std::ptr::copy_nonoverlapping(value.as_ptr(), out.cast(), value.len());
    *out.add(value.len()) = 0;
    true
}
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_store_repository_get(
    data: *const c_char,
    capacity: usize,
    out: *mut c_char,
    out_capacity: usize,
) -> bool {
    crate::guard(false, || {
        let value = read(data, capacity).and_then(|root| {
            root.get("storeRepositoryUrl")
                .and_then(Value::as_str)
                .and_then(canonical)
        });
        copy(value.as_deref().unwrap_or(DEFAULT), out, out_capacity)
    })
}
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_store_repository_set(
    data: *mut c_char,
    capacity: usize,
    text: *const c_char,
) -> bool {
    crate::guard(false, || {
        if text.is_null() {
            return false;
        }
        let Ok(text) = CStr::from_ptr(text).to_str() else {
            return false;
        };
        let Some(value) = canonical(if text.trim().is_empty() {
            DEFAULT
        } else {
            text
        }) else {
            return false;
        };
        let Some(mut root) = read(data, capacity) else {
            return false;
        };
        root.insert("storeRepositoryUrl".into(), Value::String(value));
        let Ok(json) = serde_json::to_string(&root) else {
            return false;
        };
        copy(&json, data, capacity)
    })
}
#[cfg(test)]
mod tests {
    use super::*;
    const CAP: usize = 4096;
    #[test]
    fn validates_repositories_and_preserves_settings() {
        assert_eq!(
            canonical(" https://github.com/user/models.git/ ").unwrap(),
            "https://github.com/user/models"
        );
        for bad in [
            "http://github.com/u/r",
            "https://evil.test/u/r",
            "https://u@github.com/u/r",
            "https://github.com/u/r?x",
            "https://github.com/u/r/tree/dev",
            "https://github.com/u/%2e",
            "https://github.com/u/r/../other",
        ] {
            assert!(canonical(bad).is_none(), "{bad}");
        }
        unsafe {
            let mut data = [0 as c_char; CAP];
            assert!(copy(r#"{"other":42}"#, data.as_mut_ptr(), CAP));
            assert!(bongo_safe_store_repository_set(
                data.as_mut_ptr(),
                CAP,
                c"https://github.com/u/r".as_ptr()
            ));
            let root = read(data.as_ptr(), CAP).unwrap();
            assert_eq!(root["other"], 42);
            let before = data;
            assert!(!bongo_safe_store_repository_set(
                data.as_mut_ptr(),
                CAP,
                c"invalid".as_ptr()
            ));
            assert_eq!(data, before);
            assert!(bongo_safe_store_repository_set(
                data.as_mut_ptr(),
                CAP,
                c"".as_ptr()
            ));
            let mut out = [0 as c_char; 512];
            assert!(bongo_safe_store_repository_get(
                data.as_ptr(),
                CAP,
                out.as_mut_ptr(),
                out.len()
            ));
            assert_eq!(CStr::from_ptr(out.as_ptr()).to_str().unwrap(), DEFAULT);
        }
    }
}
