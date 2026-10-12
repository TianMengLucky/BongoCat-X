//! Reduce encoded model atlases before either upstream or native GPU upload.
use image::{codecs::png::PngEncoder, imageops::FilterType, ImageEncoder};
use inox2d::model::Model;
use std::{io::Cursor, sync::Arc};

fn dimensions(width: u32, height: u32, quality: f32) -> (u32, u32) {
    let quality = if quality.is_finite() { quality.clamp(0.1, 10.0) } else { 10.0 };
    let scale = (quality / 100.0).sqrt().min(512.0 / width.max(height).max(1) as f32);
    (((width as f32 * scale).ceil() as u32).max(1).min(width),
        ((height as f32 * scale).ceil() as u32).max(1).min(height))
}

pub fn shrink_textures(model: &mut Model, quality: f32) -> Result<(), String> {
    for texture in &mut model.textures {
        let mut reader = image::ImageReader::with_format(Cursor::new(texture.data.as_ref()), texture.format);
        let mut limits = image::Limits::default();
        limits.max_alloc = Some(256 * 1024 * 1024);
        limits.max_image_width = Some(16384);
        limits.max_image_height = Some(16384);
        reader.limits(limits);
        let image = reader.decode().map_err(|e| e.to_string())?.into_rgba8();
        let (width, height) = dimensions(image.width(), image.height(), quality);
        if width == image.width() && height == image.height() { continue; }
        let resized = image::imageops::resize(&image, width, height, FilterType::Triangle);
        drop(image);
        let mut encoded = Vec::new();
        PngEncoder::new(&mut encoded).write_image(resized.as_raw(), width, height,
            image::ExtendedColorType::Rgba8).map_err(|e| e.to_string())?;
        texture.data = Arc::from(encoded);
        texture.format = image::ImageFormat::Png;
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn bounds_preserve_small_images_and_cap_large_and_invalid_inputs() {
        assert_eq!(dimensions(1, 1, 10.0), (1, 1));
        assert_eq!(dimensions(1024, 512, 10.0), (324, 162));
        assert_eq!(dimensions(16384, 8192, 10.0), (512, 256));
        assert_eq!(dimensions(1024, 512, f32::NAN), (324, 162));
        assert!(dimensions(1024, 512, 1.0).0 < 324);
    }
}
