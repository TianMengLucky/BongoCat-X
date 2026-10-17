//! Bounded PNG strips. Only borrowed output strips cross the C ABI.
use std::{
    ffi::{c_char, c_void, CStr},
    fs::File,
    io::{BufReader, Read},
    path::Path,
};
#[path = "image_rows_scale.rs"]
mod scale;
#[cfg(test)]
#[path = "image_rows_tests.rs"]
mod tests;
pub(crate) type Outcome = Result<(), i32>;
pub(crate) const FORMAT: i32 = 3;
pub(crate) const CANCELLED: i32 = 5;
pub(crate) const IO: i32 = 2;
pub(crate) const STRIP_BYTES: usize = 4 * 1024 * 1024;
pub(crate) type Consumer<'a> = dyn FnMut(&mut [u8], u32, u32, u32, u32) -> Outcome + 'a;
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Callbacks {
    rows: Option<unsafe extern "C" fn(*mut c_void, *mut u8, i32, i32, i32, i32) -> bool>,
    consumer: *mut c_void,
    progress: Option<unsafe extern "C" fn(*mut c_void, f32)>,
    userdata: *mut c_void,
    cancelled: Option<unsafe extern "C" fn(*mut c_void) -> bool>,
    cancel_data: *mut c_void,
}
impl Callbacks {
    pub(crate) fn check(self) -> Outcome {
        if self
            .cancelled
            .is_some_and(|f| unsafe { f(self.cancel_data) })
        {
            Err(CANCELLED)
        } else {
            Ok(())
        }
    }
    pub(crate) fn progress(self, value: f32) {
        if let Some(f) = self.progress {
            unsafe { f(self.userdata, value) };
        }
    }
    pub(crate) fn emit(self, pixels: &mut [u8], w: u32, rows: u32, h: u32, y: u32) -> Outcome {
        self.check()?;
        if self.rows.is_some_and(|f| unsafe {
            f(
                self.consumer,
                pixels.as_mut_ptr(),
                w as i32,
                rows as i32,
                h as i32,
                y as i32,
            )
        }) {
            Ok(())
        } else {
            Err(CANCELLED)
        }
    }
    pub(crate) fn valid(self) -> bool {
        self.rows.is_some()
    }
}
pub(crate) unsafe fn text<'a>(p: *const c_char) -> Result<&'a str, i32> {
    if p.is_null() {
        return Err(1);
    }
    CStr::from_ptr(p).to_str().map_err(|_| 1)
}
pub(crate) fn decode(
    path: &Path,
    bounds: (u32, u32),
    callbacks: Callbacks,
    consume: &mut Consumer<'_>,
) -> Outcome {
    callbacks.check()?;
    let mut input = BufReader::new(File::open(path).map_err(|_| IO)?);
    // Retain a borrow of the reader so trailing bytes can be rejected at EOF.
    let mut decoder = png::Decoder::new(&mut input);
    decoder.set_limits(png::Limits {
        bytes: 64 * 1024 * 1024,
    });
    let mut reader = decoder.read_info().map_err(|_| FORMAT)?;
    let info = reader.info();
    let (w, h) = (info.width, info.height);
    if w == 0
        || h == 0
        || w > (i32::MAX / 4) as u32
        || h > i32::MAX as u32
        || u64::from(w) * u64::from(h) * 4 > 2 * 1024 * 1024 * 1024
        || info.interlaced
        || info.bit_depth != png::BitDepth::Eight
        || info.trns.is_some()
        || !matches!(info.color_type, png::ColorType::Rgb | png::ColorType::Rgba)
    {
        return Err(FORMAT);
    }
    let channels = info.color_type.samples();
    let mut scaler = scale::Scaler::new(w, h, bounds)?;
    // RGBA rows can be borrowed: the scaler writes only to its own output strips.
    let mut rgba = vec![0; if channels == 3 { w as usize * 4 } else { 0 }];
    for y in 0..h {
        callbacks.check()?;
        let row = reader.next_row().map_err(|_| FORMAT)?.ok_or(FORMAT)?;
        if row.data().len() != w as usize * channels {
            return Err(FORMAT);
        }
        let pixels = if channels == 4 {
            row.data()
        } else {
            for (source, target) in row.data().chunks_exact(3).zip(rgba.chunks_exact_mut(4)) {
                target[..3].copy_from_slice(source);
                target[3] = 255;
            }
            &rgba
        };
        scaler.push(pixels, y, callbacks, consume)?;
        callbacks.progress((y + 1) as f32 / h as f32);
    }
    reader.finish().map_err(|_| FORMAT)?;
    drop(reader);
    let mut extra = [0];
    if input.read(&mut extra).map_err(|_| IO)? != 0 {
        return Err(FORMAT);
    }
    scaler.finish(callbacks, consume)
}
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_png_rows(
    path: *const c_char,
    width: i32,
    height: i32,
    callbacks: Callbacks,
) -> i32 {
    crate::guard(4, || {
        if width < 1 || height < 1 || !callbacks.valid() {
            return 1;
        }
        let result = text(path).and_then(|path| {
            decode(
                Path::new(path),
                (width as u32, height as u32),
                callbacks,
                &mut |pixels, w, rows, h, y| callbacks.emit(pixels, w, rows, h, y),
            )
        });
        result.err().unwrap_or(0)
    })
}
