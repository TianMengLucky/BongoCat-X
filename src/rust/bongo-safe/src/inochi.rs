//! Bounded INP/INX parsing shared by import discovery and the renderer plugin.
use std::{ffi::CStr, io::Read, os::raw::c_char, path::Path};

use image::{ImageFormat, ImageReader};
use inox2d::{formats::inp::parse_inp, model::Model};
use serde_json::Value;

#[path = "inochi_schema.rs"]
mod schema;

pub const FILE_LIMIT: u64 = 256 * 1024 * 1024;

/// Returns 1 for Cubism model3 JSON, 2 for an INP/INX container, 0 otherwise.
/// # Safety
/// `path` must be a valid NUL-terminated UTF-8 string.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_model_kind_file(path: *const c_char) -> i32 {
    crate::guard(0, || {
        if path.is_null() {
            return 0;
        }
        let Ok(path) = CStr::from_ptr(path).to_str() else {
            return 0;
        };
        let Ok(mut file) = std::fs::File::open(path) else {
            return 0;
        };
        let mut prefix = [0; 8];
        if file.read_exact(&mut prefix).is_err() {
            return 0;
        }
        if &prefix == b"TRNSRTS\0" {
            return 2;
        }
        let mut bytes = prefix.to_vec();
        if file.take(16 * 1024 * 1024).read_to_end(&mut bytes).is_err() {
            return 0;
        }
        if bytes.len() > 16 * 1024 * 1024 {
            return 0;
        }
        let doc = crate::json_dom::read::bongo_safe_model_json_parse(
            bytes.as_ptr().cast(),
            bytes.len(),
            std::ptr::null_mut(),
        );
        if doc.is_null() {
            return 0;
        }
        let json = crate::json_dom::to_value((*doc).root, 0);
        crate::json_dom::read::bongo_json_doc_free(doc);
        let Ok(json) = json else {
            return 0;
        };
        if json["Version"].as_u64().is_some_and(|v| v >= 3)
            && json["FileReferences"]["Moc"]
                .as_str()
                .is_some_and(|s| !s.is_empty())
        {
            1
        } else {
            0
        }
    })
}

pub fn read_file(path: &Path) -> Result<Vec<u8>, String> {
    let file = std::fs::File::open(path).map_err(|e| e.to_string())?;
    let mut bytes = Vec::new();
    file.take(FILE_LIMIT + 1)
        .read_to_end(&mut bytes)
        .map_err(|e| e.to_string())?;
    if bytes.len() as u64 > FILE_LIMIT {
        return Err("INP exceeds 256 MiB".into());
    }
    Ok(bytes)
}

struct Cursor<'a> {
    bytes: &'a [u8],
    offset: usize,
}
impl<'a> Cursor<'a> {
    fn take(&mut self, count: usize) -> Result<&'a [u8], String> {
        let end = self
            .offset
            .checked_add(count)
            .ok_or("INP length overflow")?;
        let value = self
            .bytes
            .get(self.offset..end)
            .ok_or("Truncated INP section")?;
        self.offset = end;
        Ok(value)
    }
    fn size(&mut self, limit: usize) -> Result<usize, String> {
        let count = u32::from_be_bytes(self.take(4)?.try_into().unwrap()) as usize;
        if count > limit {
            return Err("INP section exceeds its size limit".into());
        }
        Ok(count)
    }
    fn blob(&mut self, limit: usize) -> Result<&'a [u8], String> {
        let size = self.size(limit)?;
        self.take(size)
    }
}

pub fn payload(bytes: &[u8]) -> Result<Value, String> {
    let mut cursor = Cursor { bytes, offset: 0 };
    if bytes.len() as u64 > FILE_LIMIT || cursor.take(8)? != b"TRNSRTS\0" {
        return Err("Not an Inochi2D INP/INX container".into());
    }
    let json: Value = serde_json::from_slice(cursor.blob(16 * 1024 * 1024)?)
        .map_err(|e| format!("Invalid INP JSON: {e}"))?;
    if cursor.take(8)? != b"TEX_SECT" {
        return Err("Missing INP texture section".into());
    }
    let count = cursor.size(256)?;
    let mut pixels = 0u64;
    for _ in 0..count {
        let length = cursor.size(64 * 1024 * 1024)?;
        let format = match cursor.take(1)?[0] {
            0 => ImageFormat::Png,
            1 => ImageFormat::Tga,
            _ => return Err("Inox2D supports PNG/TGA textures; BC7 is unsupported".into()),
        };
        let texture = cursor.take(length)?;
        let reader = ImageReader::with_format(std::io::Cursor::new(texture), format);
        let (width, height) = reader.into_dimensions().map_err(|e| e.to_string())?;
        pixels += u64::from(width) * u64::from(height) * 4;
        if width == 0 || height == 0 || width > 16384 || height > 16384 || pixels > FILE_LIMIT {
            return Err("INP decoded textures exceed the memory/size limit".into());
        }
    }
    if cursor.offset != bytes.len() {
        if cursor.take(8)? != b"EXT_SECT" {
            return Err("Unknown INP trailing section".into());
        }
        for _ in 0..cursor.size(64)? {
            std::str::from_utf8(cursor.blob(1024)?).map_err(|e| e.to_string())?;
            serde_json::from_slice::<Value>(cursor.blob(1024 * 1024)?)
                .map_err(|e| e.to_string())?;
        }
        if cursor.offset != bytes.len() {
            return Err("Unexpected INP trailing bytes".into());
        }
    }
    schema::validate(&json, count)?;
    Ok(json)
}

pub fn parse(bytes: &[u8]) -> Result<(Model, Value), String> {
    let json = payload(bytes)?;
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        let mut model = parse_inp(bytes).map_err(|e| e.to_string())?;
        model.puppet.init_transforms();
        model.puppet.init_rendering();
        model.puppet.init_params();
        model.puppet.init_physics();
        model.puppet.begin_frame();
        model.puppet.end_frame(0.0);
        Ok::<Model, String>(model)
    }))
    .map_err(|_| "Inox2D rejected the model structure".to_string())?;
    Ok((result?, json))
}

/// # Safety
/// Strings/buffers must be valid for their stated lengths. `message` is optional.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_inochi_validate_file(
    path: *const c_char,
    message: *mut c_char,
    capacity: usize,
) -> bool {
    crate::guard(false, || {
        let result = if path.is_null() {
            Err("Missing model path".into())
        } else {
            CStr::from_ptr(path)
                .to_str()
                .map_err(|e| e.to_string())
                .and_then(|path| read_file(Path::new(path)))
                .and_then(|bytes| parse(&bytes).map(|_| ()))
        };
        if !message.is_null() && capacity > 0 {
            let text = result.as_ref().err().map(String::as_bytes).unwrap_or(b"");
            let count = text.len().min(capacity - 1);
            std::ptr::copy_nonoverlapping(text.as_ptr(), message.cast(), count);
            *message.add(count) = 0;
        }
        result.is_ok()
    })
}

#[cfg(test)]
#[path = "inochi_tests.rs"]
mod tests;
