//! Locale bytes and translation allocations live in each plugin library.
use std::collections::BTreeMap;
use std::ffi::{c_char, c_void, CStr, CString};

#[repr(C)]
pub struct Api {
    pub abi_version: u32,
    pub struct_size: u32,
    pub prefix: *const c_char,
    pub extra_prefix: *const c_char,
    pub load: unsafe extern "C" fn(*const c_char) -> u32,
    pub text: unsafe extern "C" fn(*const c_char, *const c_char) -> *const c_char,
    pub visit:
        unsafe extern "C" fn(*mut c_void, Option<unsafe extern "C" fn(*mut c_void, *const c_char)>),
    pub release: extern "C" fn(),
}
// SAFETY: exported tables contain only immutable static strings and functions.
unsafe impl Sync for Api {}

pub struct Locale {
    pub language: String,
    strings: BTreeMap<String, CString>,
}
impl Locale {
    pub fn load(language: &str, bytes: &str) -> Option<Self> {
        if bytes.len() > 1024 * 1024 {
            return None;
        }
        let strings: BTreeMap<String, String> = serde_json::from_str(bytes).ok()?;
        let strings = strings
            .into_iter()
            .map(|(key, value)| CString::new(value).map(|v| (key, v)))
            .collect::<Result<_, _>>()
            .ok()?;
        Some(Self {
            language: language.to_owned(),
            strings,
        })
    }
    /// Returned strings are borrowed until load/release on the main thread.
    pub fn get(&self, key: &str) -> Option<&CStr> {
        self.strings.get(key).map(CString::as_c_str)
    }
    pub fn visit(&self, mut visitor: impl FnMut(&CStr)) {
        for text in self.strings.values() {
            visitor(text);
        }
    }
}

/// Defines the plugin's explicit locale loading interface and private cache.
#[macro_export]
macro_rules! export_locales {
    ($symbol:ident, $prefix:literal, $extra:literal) => {
        static LOCALE: std::sync::Mutex<Option<$crate::locale::Locale>> =
            std::sync::Mutex::new(None);
        unsafe extern "C" fn load_locale(language: *const std::ffi::c_char) -> u32 {
            $crate::guard::guard(0, || {
                if language.is_null() {
                    return 0;
                }
                let requested = std::ffi::CStr::from_ptr(language)
                    .to_str()
                    .unwrap_or("en-US");
                let (language, bytes) = match requested {
                    "zh-CN" => (
                        "zh-CN",
                        include_str!(concat!(env!("CARGO_MANIFEST_DIR"), "/locales/zh-CN.json")),
                    ),
                    "zh-Hant" => (
                        "zh-Hant",
                        include_str!(concat!(env!("CARGO_MANIFEST_DIR"), "/locales/zh-Hant.json")),
                    ),
                    "ja-JP" => (
                        "ja-JP",
                        include_str!(concat!(env!("CARGO_MANIFEST_DIR"), "/locales/ja-JP.json")),
                    ),
                    "ko-KR" => (
                        "ko-KR",
                        include_str!(concat!(env!("CARGO_MANIFEST_DIR"), "/locales/ko-KR.json")),
                    ),
                    "de-DE" => (
                        "de-DE",
                        include_str!(concat!(env!("CARGO_MANIFEST_DIR"), "/locales/de-DE.json")),
                    ),
                    "es-ES" => (
                        "es-ES",
                        include_str!(concat!(env!("CARGO_MANIFEST_DIR"), "/locales/es-ES.json")),
                    ),
                    "fr-FR" => (
                        "fr-FR",
                        include_str!(concat!(env!("CARGO_MANIFEST_DIR"), "/locales/fr-FR.json")),
                    ),
                    "pt-BR" => (
                        "pt-BR",
                        include_str!(concat!(env!("CARGO_MANIFEST_DIR"), "/locales/pt-BR.json")),
                    ),
                    "ru-RU" => (
                        "ru-RU",
                        include_str!(concat!(env!("CARGO_MANIFEST_DIR"), "/locales/ru-RU.json")),
                    ),
                    _ => (
                        "en-US",
                        include_str!(concat!(env!("CARGO_MANIFEST_DIR"), "/locales/en-US.json")),
                    ),
                };
                let Ok(mut locale) = LOCALE.lock() else {
                    return 0;
                };
                if locale.as_ref().is_some_and(|l| l.language == language) {
                    return 1;
                }
                let Some(next) = $crate::locale::Locale::load(language, bytes) else {
                    return 0;
                };
                *locale = Some(next);
                1
            })
        }
        unsafe extern "C" fn locale_text(
            key: *const std::ffi::c_char,
            fallback: *const std::ffi::c_char,
        ) -> *const std::ffi::c_char {
            $crate::guard::guard(fallback, || {
                if key.is_null() {
                    return fallback;
                }
                let Ok(key) = std::ffi::CStr::from_ptr(key).to_str() else {
                    return fallback;
                };
                let Ok(locale) = LOCALE.lock() else {
                    return fallback;
                };
                locale
                    .as_ref()
                    .and_then(|l| l.get(key))
                    .map_or(fallback, |s| s.as_ptr())
            })
        }
        unsafe extern "C" fn visit_locale(
            context: *mut std::ffi::c_void,
            visitor: Option<unsafe extern "C" fn(*mut std::ffi::c_void, *const std::ffi::c_char)>,
        ) {
            $crate::guard::guard((), || {
                if let (Some(visitor), Ok(locale)) = (visitor, LOCALE.lock()) {
                    if let Some(locale) = locale.as_ref() {
                        locale.visit(|text| visitor(context, text.as_ptr()));
                    }
                }
            });
        }
        extern "C" fn release_locale() {
            $crate::guard::guard((), || {
                if let Ok(mut locale) = LOCALE.lock() {
                    *locale = None;
                }
            });
        }
        static LOCALE_API: $crate::locale::Api = $crate::locale::Api {
            abi_version: 1,
            struct_size: std::mem::size_of::<$crate::locale::Api>() as u32,
            prefix: concat!($prefix, "\0").as_ptr().cast(),
            extra_prefix: concat!($extra, "\0").as_ptr().cast(),
            load: load_locale,
            text: locale_text,
            visit: visit_locale,
            release: release_locale,
        };
        #[no_mangle]
        pub extern "C" fn $symbol(abi: u32) -> *const $crate::locale::Api {
            if abi == 1 {
                &LOCALE_API
            } else {
                std::ptr::null()
            }
        }
    };
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn loading_owns_strings_and_rejects_bad_input() {
        let bytes = String::from(r#"{"greeting":"你好"}"#);
        let locale = Locale::load("zh-CN", &bytes).unwrap();
        drop(bytes);
        assert_eq!(locale.get("greeting").unwrap().to_str().unwrap(), "你好");
        assert!(locale.get("unknown").is_none());
        assert!(Locale::load("bad", r#"{"bad":"\u0000"}"#).is_none());
        assert!(Locale::load("bad", "[]").is_none());
        assert_eq!(
            std::mem::size_of::<Api>(),
            8 + 6 * std::mem::size_of::<usize>()
        );
    }
}
