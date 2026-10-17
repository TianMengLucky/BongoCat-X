//! Pure version/encoding helpers; the host never walks untrusted UTF-8 bytes.
use std::{
    cmp::Ordering,
    ffi::{c_char, CStr},
};
#[derive(Debug)]
struct Version<'a> {
    core: [u64; 3],
    prerelease: Option<&'a str>,
}
fn identifiers(value: &str) -> bool {
    value.split('.').all(|p| {
        !p.is_empty()
            && p.bytes().all(|b| b.is_ascii_alphanumeric() || b == b'-')
            && !(p.len() > 1 && p.starts_with('0') && p.bytes().all(|b| b.is_ascii_digit()))
    })
}
fn parse(value: &str) -> Option<Version<'_>> {
    let value = value.strip_prefix(['v', 'V']).unwrap_or(value);
    let (value, build) = value
        .split_once('+')
        .map_or((value, None), |(a, b)| (a, Some(b)));
    if build.is_some_and(|v| !identifiers(v)) {
        return None;
    }
    let (core, prerelease) = value
        .split_once('-')
        .map_or((value, None), |(a, b)| (a, Some(b)));
    if prerelease.is_some_and(|v| !identifiers(v)) {
        return None;
    }
    let mut parts = core.split('.');
    let mut core = [0; 3];
    for number in &mut core {
        let text = parts.next()?;
        if text.is_empty()
            || !text.bytes().all(|b| b.is_ascii_digit())
            || (text.len() > 1 && text.starts_with('0'))
        {
            return None;
        }
        *number = text.parse().ok()?;
    }
    if parts.next().is_some() {
        return None;
    }
    Some(Version { core, prerelease })
}
fn compare(left: Version<'_>, right: Version<'_>) -> Ordering {
    let core = left.core.cmp(&right.core);
    if core != Ordering::Equal {
        return core;
    }
    let (Some(left), Some(right)) = (left.prerelease, right.prerelease) else {
        return left.prerelease.is_none().cmp(&right.prerelease.is_none());
    };
    let mut left = left.split('.');
    let mut right = right.split('.');
    loop {
        let (a, b) = (left.next(), right.next());
        let (Some(a), Some(b)) = (a, b) else {
            return a.is_some().cmp(&b.is_some());
        };
        let an = a.bytes().all(|b| b.is_ascii_digit());
        let bn = b.bytes().all(|b| b.is_ascii_digit());
        let order = match (an, bn) {
            (true, true) => a.len().cmp(&b.len()).then_with(|| a.cmp(b)),
            (true, false) => Ordering::Less,
            (false, true) => Ordering::Greater,
            _ => a.cmp(b),
        };
        if order != Ordering::Equal {
            return order;
        }
    }
}
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_version_valid(text: *const c_char) -> bool {
    crate::guard(false, || {
        crate::image_rows::text(text).ok().and_then(parse).is_some()
    })
}
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_version_compare(
    left: *const c_char,
    right: *const c_char,
) -> i32 {
    crate::guard(0, || {
        let versions = crate::image_rows::text(left)
            .ok()
            .and_then(parse)
            .zip(crate::image_rows::text(right).ok().and_then(parse));
        versions.map_or(0, |(a, b)| match compare(a, b) {
            Ordering::Less => -1,
            Ordering::Equal => 0,
            Ordering::Greater => 1,
        })
    })
}
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_utf8_valid(text: *const c_char) -> bool {
    crate::guard(false, || {
        !text.is_null() && CStr::from_ptr(text).to_str().is_ok()
    })
}
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_utf8_normalize(
    text: *const c_char,
    output: *mut c_char,
    capacity: usize,
) -> bool {
    if text.is_null() || output.is_null() || capacity == 0 {
        return false;
    }
    *output = 0;
    crate::guard(false, || {
        let bytes = CStr::from_ptr(text).to_bytes();
        let value = match std::str::from_utf8(bytes) {
            Ok(value) if value.len() < capacity => value.to_owned(),
            Ok(_) => return false,
            Err(_) if cfg!(windows) && bytes.len() < 128 => {
                let Ok(mut value) = crate::archive::legacy_name(bytes) else {
                    return false;
                };
                let mut length = value.len().min(capacity - 1);
                while !value.is_char_boundary(length) {
                    length -= 1;
                }
                value.truncate(length);
                value
            }
            Err(_) => return false,
        };
        std::ptr::copy_nonoverlapping(value.as_ptr(), output.cast(), value.len());
        *output.add(value.len()) = 0;
        true
    })
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn version_ordering_handles_unbounded_prerelease_numbers() {
        assert_eq!(
            compare(
                parse("v1.2.3-999999999999999999999").unwrap(),
                parse("1.2.3-1000000000000000000000").unwrap()
            ),
            Ordering::Less
        );
        assert_eq!(
            compare(parse("1.2.3-rc.1").unwrap(), parse("1.2.3").unwrap()),
            Ordering::Less
        );
        for value in ["1.2", "01.2.3", "1.2.3-01", "1.2.3+", "1.2.3-α"] {
            assert!(parse(value).is_none());
        }
    }
}
