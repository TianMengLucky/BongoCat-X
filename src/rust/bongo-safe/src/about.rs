//! The about-page contributor feed, replacing the nanosvg XML parser and the
//! hand-rolled base64/avatar decoding in `preferences_about_feed.c`.
//!
//! The input is the self-assembled SVG the application builds from the
//! GitHub contributors API: `<a href="…"><title>name</title><image
//! xlink:href="data:image/...;base64,…"/></a>` records. The avatar bytes are
//! contributor-controlled network data, so parsing and decoding happen here.

use crate::{guard, release_exact};
use base64::Engine;
use std::ffi::CString;
use std::os::raw::{c_char, c_int, c_void};

/// One parsed contributor. `name` and `profile` are released by
/// `bongo_safe_free_persons`; `pixels` (`avatar_size * avatar_size * 4`)
/// passes to the caller.
#[repr(C)]
pub struct BongoSafePerson {
    pub name: *mut c_char,
    pub profile: *mut c_char,
    pub pixels: *mut u8,
}

const BASE64_LIMIT: usize = 1024 * 1024;
const PROFILE_LIMIT: usize = 1024;
const NAME_LIMIT: usize = 255;
const MAX_DIMENSION: i64 = 4096;
const MAX_PIXELS: i64 = 4 * 1024 * 1024;
const MAX_XML_DEPTH: usize = 32;
const XLINK_NS: &str = "http://www.w3.org/1999/xlink";
const URI_PREFIXES: [&str; 3] = [
    "data:image/png;base64,",
    "data:image/jpeg;base64,",
    "data:image/webp;base64,",
];

/// The C parser matched the literal attribute names "href" and
/// "xlink:href"; resolve either shape here.
fn href_attribute<'a, 'input>(node: roxmltree::Node<'a, 'input>) -> Option<&'a str> {
    node.attributes().find_map(|attribute| {
        if attribute.name() != "href" {
            return None;
        }
        match attribute.namespace() {
            None => Some(attribute.value()),
            Some(ns) if ns == XLINK_NS => Some(attribute.value()),
            _ => None,
        }
    })
}

type Cancelled = Option<unsafe extern "C" fn(*mut c_void) -> bool>;

struct FeedState {
    cancelled: Cancelled,
    userdata: *mut c_void,
    avatar_size: usize,
    cap: usize,
    people: Vec<Draft>,
}

impl FeedState {
    fn check_cancel(&self) -> Result<(), Fail> {
        if let Some(check) = self.cancelled {
            if unsafe { check(self.userdata) } {
                return Err(Fail::Cancelled);
            }
        }
        Ok(())
    }
}

struct Draft {
    name: Option<String>,
    profile: Option<String>,
    pixels: Option<Vec<u8>>,
}

enum Fail {
    Cancelled,
    Malformed,
}

/// # Safety
/// `svg` must be readable for `size` bytes; `out_persons` writable. On
/// success `*out_persons` holds `count` persons whose `name`/`profile` are
/// released by `bongo_safe_free_persons` and whose `pixels` pass to the
/// caller. Returns the count, or -1 on failure or cancellation (leaving
/// `*out_persons` untouched).
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_about_feed_parse(
    svg: *const c_char,
    size: usize,
    cancelled: Cancelled,
    userdata: *mut c_void,
    avatar_size: c_int,
    person_cap: c_int,
    out_persons: *mut *mut BongoSafePerson,
) -> c_int {
    guard(-1, || {
        if svg.is_null() || out_persons.is_null() || avatar_size <= 0 || person_cap <= 0 {
            return -1;
        }
        let text = match std::str::from_utf8(std::slice::from_raw_parts(svg.cast::<u8>(), size)) {
            Ok(text) => text,
            Err(_) => return -1,
        };
        let mut state = FeedState {
            cancelled,
            userdata,
            avatar_size: avatar_size as usize,
            cap: person_cap as usize,
            people: Vec::new(),
        };
        match parse_feed(text, &mut state) {
            Ok(()) => {
                let count = state.people.len() as c_int;
                *out_persons = publish(state.people);
                count
            }
            Err(_) => -1,
        }
    })
}

/// Releases the person array together with every `name` and `profile`.
/// The `pixels` buffers pass to the caller before this call.
///
/// # Safety
/// `persons` must come from `bongo_safe_about_feed_parse` with the same
/// count and must not have been released yet.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_free_persons(persons: *mut BongoSafePerson, count: c_int) {
    guard((), || {
        if persons.is_null() || count <= 0 {
            return;
        }
        let count = count as usize;
        let slice = std::slice::from_raw_parts(persons, count);
        for person in slice {
            if !person.name.is_null() {
                drop(CString::from_raw(person.name));
            }
            if !person.profile.is_null() {
                drop(CString::from_raw(person.profile));
            }
        }
        drop(crate::reclaim_exact(persons, count));
    })
}

fn parse_feed(text: &str, state: &mut FeedState) -> Result<(), Fail> {
    // Cheap pre-checks keep parity with the C parser: no DTDs, ever.
    if !text.contains("<svg") || text.contains("<!DOCTYPE") || text.contains("<!ENTITY") {
        return Err(Fail::Malformed);
    }
    let options = roxmltree::ParsingOptions {
        nodes_limit: 100_000,
        ..roxmltree::ParsingOptions::default()
    };
    let document =
        roxmltree::Document::parse_with_options(text, options).map_err(|_| Fail::Malformed)?;
    walk(document.root(), 0, false, &mut None, state)?;
    state.check_cancel()?;
    Ok(())
}

fn walk(
    node: roxmltree::Node,
    depth: usize,
    in_link: bool,
    draft: &mut Option<Draft>,
    state: &mut FeedState,
) -> Result<(), Fail> {
    if depth > MAX_XML_DEPTH {
        return Err(Fail::Malformed);
    }
    for child in node.children() {
        if !child.is_element() {
            continue;
        }
        state.check_cancel()?;
        match child.tag_name().name() {
            "a" => {
                if in_link {
                    // The C parser rejects nested links outright.
                    return Err(Fail::Malformed);
                }
                let href = href_attribute(child).unwrap_or("");
                let mut slot = Some(Draft {
                    name: None,
                    profile: if href.starts_with("https://") && href.len() < PROFILE_LIMIT {
                        Some(href.to_string())
                    } else {
                        None
                    },
                    pixels: None,
                });
                walk(child, depth + 1, true, &mut slot, state)?;
                if let Some(person) = slot {
                    if person.pixels.is_some() && state.people.len() < state.cap {
                        state.people.push(person);
                    }
                }
            }
            "title" if in_link => {
                if let Some(person) = draft.as_mut() {
                    person.name = child.text().map(|text| text.to_string());
                }
            }
            "image" if in_link => {
                if let Some(person) = draft.as_mut() {
                    if person.pixels.is_none() && state.people.len() < state.cap {
                        let href = href_attribute(child).unwrap_or("");
                        state.check_cancel()?;
                        person.pixels = decode_avatar(href, state.avatar_size);
                    }
                }
            }
            _ => walk(child, depth + 1, in_link, draft, state)?,
        }
    }
    Ok(())
}

fn decode_avatar(uri: &str, avatar_size: usize) -> Option<Vec<u8>> {
    let encoded = URI_PREFIXES
        .iter()
        .find_map(|prefix| uri.strip_prefix(prefix))?;
    if encoded.is_empty() || encoded.len() > BASE64_LIMIT {
        return None;
    }
    let bytes = base64::engine::general_purpose::STANDARD
        .decode(encoded)
        .ok()?;
    let mut reader = image::ImageReader::new(std::io::Cursor::new(&bytes));
    reader = reader.with_guessed_format().ok()?;
    let mut limits = image::Limits::default();
    limits.max_image_width = Some(MAX_DIMENSION as u32);
    limits.max_image_height = Some(MAX_DIMENSION as u32);
    limits.max_alloc = Some((MAX_PIXELS * 4) as u64);
    reader.limits(limits);
    let decoded = reader.decode().ok()?.into_rgba8();
    let (width, height) = decoded.dimensions();
    if width == 0
        || height == 0
        || width > MAX_DIMENSION as u32
        || height > MAX_DIMENSION as u32
        || width as i64 * height as i64 > MAX_PIXELS
    {
        return None;
    }
    let resized = image::imageops::resize(
        &decoded,
        avatar_size as u32,
        avatar_size as u32,
        image::imageops::FilterType::Nearest,
    );
    let mut pixels = resized.into_raw();
    // Same circular mask as the C implementation: keep the inscribed circle.
    let size = avatar_size as i32;
    for y in 0..size {
        for x in 0..size {
            let dx = 2 * x + 1 - size;
            let dy = 2 * y + 1 - size;
            if dx * dx + dy * dy > size * size {
                pixels[(y as usize * avatar_size + x as usize) * 4 + 3] = 0;
            }
        }
    }
    Some(pixels)
}

fn publish(people: Vec<Draft>) -> *mut BongoSafePerson {
    let mut persons: Vec<BongoSafePerson> = Vec::with_capacity(people.len());
    for person in people {
        let name = match &person.name {
            Some(name) if !name.is_empty() => truncated(name, NAME_LIMIT),
            _ => CString::new("Contributor").unwrap(),
        };
        let profile = CString::new(person.profile.as_deref().unwrap_or("")).unwrap_or_default();
        let pixels = match person.pixels {
            Some(pixels) => release_exact(pixels),
            None => std::ptr::null_mut(),
        };
        persons.push(BongoSafePerson {
            name: name.into_raw(),
            profile: profile.into_raw(),
            pixels,
        });
    }
    // Preserve the struct alignment and transfer the array without a byte copy.
    release_exact(persons)
}

fn truncated(text: &str, limit: usize) -> CString {
    let mut bytes = 0;
    let mut taken = 0;
    for character in text.chars() {
        let length = character.len_utf8();
        if bytes + length > limit {
            break;
        }
        bytes += length;
        taken += 1;
    }
    CString::new(text.chars().take(taken).collect::<String>()).unwrap_or_default()
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::image::bongo_safe_free_pixels;

    extern "C" fn not_cancelled(_: *mut c_void) -> bool {
        false
    }

    extern "C" fn cancelled(_: *mut c_void) -> bool {
        true
    }

    /// 1x1 opaque red PNG, the same bytes the image module tests use.
    const RED_PNG: &[u8] = &[
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44,
        0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x02, 0x00, 0x00, 0x00, 0x90,
        0x77, 0x53, 0xDE, 0x00, 0x00, 0x00, 0x0C, 0x49, 0x44, 0x41, 0x54, 0x08, 0xD7, 0x63, 0xF8,
        0xCF, 0xC0, 0x00, 0x00, 0x03, 0x01, 0x01, 0x00, 0x18, 0xDD, 0x8D, 0xB0, 0x00, 0x00, 0x00,
        0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82,
    ];

    pub(super) fn red_png_base64() -> String {
        base64::engine::general_purpose::STANDARD.encode(RED_PNG)
    }

    #[test]
    fn parses_the_assembled_feed() {
        let red_png = red_png_base64();
        let svg = format!(
            "<svg xmlns=\"http://www.w3.org/2000/svg\" \
             xmlns:xlink=\"http://www.w3.org/1999/xlink\">\
             <a href=\"https://github.com/octo\"><title>O&amp;cto</title>\
             <image xlink:href=\"data:image/png;base64,{red_png}\"/></a>\
             <a href=\"https://github.com/empty\"><title>Nobody</title></a>\
             </svg>"
        );
        let mut persons: *mut BongoSafePerson = std::ptr::null_mut();
        let count = unsafe {
            bongo_safe_about_feed_parse(
                svg.as_ptr().cast(),
                svg.len(),
                Some(not_cancelled),
                std::ptr::null_mut(),
                8,
                64,
                &mut persons,
            )
        };
        assert_eq!(count, 1, "contributors without avatars are dropped");
        let person = unsafe { std::ptr::read(persons) };
        // Borrow the strings only; bongo_safe_free_persons releases them.
        let name = unsafe { std::ffi::CStr::from_ptr(person.name) };
        let profile = unsafe { std::ffi::CStr::from_ptr(person.profile) };
        assert_eq!(name.to_str().unwrap(), "O&cto");
        assert_eq!(profile.to_str().unwrap(), "https://github.com/octo");
        let pixels = unsafe { std::slice::from_raw_parts(person.pixels, 8 * 8 * 4) };
        assert_eq!(
            pixels[(4 * 8 + 4) * 4 + 3],
            255,
            "the circle covers the center"
        );
        assert_eq!(pixels[3], 0, "corners fall outside the inscribed circle");
        unsafe { bongo_safe_free_pixels(person.pixels, 8 * 8 * 4) };
        unsafe { bongo_safe_free_persons(persons, count) };
    }

    #[test]
    fn rejects_nested_links_and_doctypes() {
        let nested = "<svg><a href=\"https://a\"><a href=\"https://b\"/></a></svg>";
        let mut persons: *mut BongoSafePerson = std::ptr::null_mut();
        let count = unsafe {
            bongo_safe_about_feed_parse(
                nested.as_ptr().cast(),
                nested.len(),
                Some(not_cancelled),
                std::ptr::null_mut(),
                8,
                64,
                &mut persons,
            )
        };
        assert_eq!(count, -1);
        let doctype = "<!DOCTYPE svg><svg><a href=\"https://a\"/></svg>";
        let count = unsafe {
            bongo_safe_about_feed_parse(
                doctype.as_ptr().cast(),
                doctype.len(),
                Some(not_cancelled),
                std::ptr::null_mut(),
                8,
                64,
                &mut persons,
            )
        };
        assert_eq!(count, -1);
    }

    #[test]
    fn cancellation_fails_the_whole_parse() {
        let red_png = red_png_base64();
        let svg = format!(
            "<svg><a href=\"https://github.com/octo\"><title>x</title>\
             <image xlink:href=\"data:image/png;base64,{red_png}\"/></a></svg>"
        );
        let mut persons: *mut BongoSafePerson = std::ptr::null_mut();
        let count = unsafe {
            bongo_safe_about_feed_parse(
                svg.as_ptr().cast(),
                svg.len(),
                Some(cancelled),
                std::ptr::null_mut(),
                8,
                64,
                &mut persons,
            )
        };
        assert_eq!(count, -1);
    }

    #[test]
    fn transferred_avatars_remain_owned_by_the_caller_after_shell_release() {
        let mut pixels = Vec::with_capacity(128);
        pixels.extend([9, 21, 43, 67]);
        let people = publish(vec![Draft {
            name: Some("Test".into()),
            profile: None,
            pixels: Some(pixels),
        }]);
        assert_eq!(
            (people as usize) % std::mem::align_of::<BongoSafePerson>(),
            0
        );
        let pixels = unsafe { (*people).pixels };
        unsafe { bongo_safe_free_persons(people, 1) };
        assert_eq!(
            unsafe { std::slice::from_raw_parts(pixels, 4) },
            [9, 21, 43, 67]
        );
        unsafe { bongo_safe_free_pixels(pixels, 4) };
    }

    #[test]
    fn free_persons_only_touches_the_shell() {
        // Freeing an empty array must not touch memory.
        unsafe { bongo_safe_free_persons(std::ptr::null_mut(), 0) };
        unsafe { bongo_safe_free_persons(std::ptr::null_mut(), 3) };
    }
}
