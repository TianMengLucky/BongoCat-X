use super::{alpha_row, fitted, Texture, ALPHA_BYTES, MAX_DECODE_BYTES, MAX_DIMENSION};
use std::{fs::File, io::BufReader, path::Path};

/// Decode noninterlaced PNGs with one source row and two output-width float rows.
/// Downsampling integrates premultiplied colors over each destination pixel.
pub(super) fn decode(
    path: &Path,
    max_w: u32,
    max_h: u32,
    bc7: bool,
    stopped: &dyn Fn() -> bool,
) -> Option<Texture> {
    let mut decoder = png::Decoder::new(BufReader::new(File::open(path).ok()?));
    decoder.set_limits(png::Limits {
        bytes: 64 * 1024 * 1024,
    });
    decoder.set_transformations(png::Transformations::EXPAND | png::Transformations::STRIP_16);
    let mut reader = decoder.read_info().ok()?;
    let info = reader.info();
    let (sw, sh) = (info.width, info.height);
    if info.interlaced || sw == 0 || sh == 0 || sw > MAX_DIMENSION || sh > MAX_DIMENSION {
        return None;
    }
    if u64::from(sw) * u64::from(sh) * 4 > MAX_DECODE_BYTES {
        return None;
    }
    let (color, depth) = reader.output_color_type();
    if depth != png::BitDepth::Eight {
        return None;
    }
    let channels = color.samples();
    let (w, h) = fitted(sw, sh, max_w, max_h);
    let size = (w as usize).checked_mul(h as usize)?.checked_mul(4)?;
    if size as u64 > MAX_DECODE_BYTES {
        return None;
    }
    let compressed =
        bc7 && cfg!(all(
            target_arch = "x86_64",
            any(target_os = "windows", target_os = "linux")
        )) && (w.div_ceil(4) as usize * h.div_ceil(4) as usize * 16) < size;
    #[cfg(all(
        target_arch = "x86_64",
        any(target_os = "windows", target_os = "linux")
    ))]
    let mut encoder = compressed.then(|| super::bc7_stream::Encoder::new(w, h));
    let mut texture = Texture {
        pixels: if compressed {
            Vec::new()
        } else {
            vec![0; size]
        },
        width: w,
        height: h,
        source_width: sw,
        source_height: sh,
        format: 0,
        alpha: [0; ALPHA_BYTES],
    };
    let mut rgba = vec![0; sw as usize * 4];
    let mut horizontal = vec![0f64; w as usize * 4];
    let mut sum = vec![0f64; w as usize * 4];
    let mut output = vec![0; w as usize * 4];
    let sx = f64::from(sw) / f64::from(w);
    let sy = f64::from(sh) / f64::from(h);
    let mut oy = 0u32;
    for y in 0..sh {
        if stopped() {
            return None;
        }
        let row = reader.next_row().ok()??;
        if row.data().len() != sw as usize * channels {
            return None;
        }
        for (source, target) in row
            .data()
            .chunks_exact(channels)
            .zip(rgba.chunks_exact_mut(4))
        {
            match color {
                png::ColorType::Rgb => {
                    target[..3].copy_from_slice(source);
                    target[3] = 255;
                }
                png::ColorType::Rgba => target.copy_from_slice(source),
                png::ColorType::Grayscale => {
                    target[..3].fill(source[0]);
                    target[3] = 255;
                }
                png::ColorType::GrayscaleAlpha => {
                    target[..3].fill(source[0]);
                    target[3] = source[1];
                }
                _ => return None,
            }
        }
        alpha_row(&mut texture.alpha, &rgba, sw, sh, y);
        horizontal.fill(0.0);
        for x in 0..w as usize {
            let left = x as f64 * sx;
            let right = (x + 1) as f64 * sx;
            for ix in left.floor() as usize..(right.ceil() as usize).min(sw as usize) {
                let weight = (right.min((ix + 1) as f64) - left.max(ix as f64)) / sx;
                let pixel = &rgba[ix * 4..ix * 4 + 4];
                let alpha = f64::from(pixel[3]) / 255.0;
                for c in 0..3 {
                    horizontal[x * 4 + c] += f64::from(pixel[c]) * alpha * weight;
                }
                horizontal[x * 4 + 3] += f64::from(pixel[3]) * weight;
            }
        }
        let mut top = f64::from(y);
        let bottom = f64::from(y + 1);
        while top < bottom && oy < h {
            let edge = (f64::from(oy + 1) * sy).min(bottom);
            let weight = (edge - top) / sy;
            for (sum, value) in sum.iter_mut().zip(&horizontal) {
                *sum += value * weight;
            }
            top = edge;
            if edge >= f64::from(oy + 1) * sy || y + 1 == sh {
                let offset = oy as usize * w as usize * 4;
                for (target, value) in output.iter_mut().zip(&sum) {
                    *target = value.round().clamp(0.0, 255.0) as u8;
                }
                #[cfg(all(
                    target_arch = "x86_64",
                    any(target_os = "windows", target_os = "linux")
                ))]
                if let Some(encoder) = &mut encoder {
                    if stopped() {
                        return None;
                    }
                    encoder.push(&output);
                }
                if !compressed {
                    texture.pixels[offset..offset + output.len()].copy_from_slice(&output);
                }
                sum.fill(0.0);
                oy += 1;
            }
        }
    }
    reader.finish().ok()?;
    if oy != h || stopped() {
        return None;
    }
    #[cfg(all(
        target_arch = "x86_64",
        any(target_os = "windows", target_os = "linux")
    ))]
    if let Some(encoder) = encoder {
        texture.pixels = encoder.finish();
        texture.format = 1;
    }
    Some(texture)
}
