//! BC7 output with one padded four-row RGBA strip, shared by both decoders.
use intel_tex_2::{bc7, RgbaSurface};

pub(super) struct Encoder {
    rows: Vec<u8>,
    blocks: Vec<u8>,
    width: usize,
    height: usize,
    row: usize,
    settings: bc7::EncodeSettings,
}

impl Encoder {
    pub(super) fn new(width: u32, height: u32) -> Self {
        let width = width as usize;
        let height = height as usize;
        Self {
            rows: vec![0; width.div_ceil(4) * 16 * 4],
            blocks: vec![0; width.div_ceil(4) * height.div_ceil(4) * 16],
            width,
            height,
            row: 0,
            settings: bc7::alpha_basic_settings(),
        }
    }

    pub(super) fn push(&mut self, pixels: &[u8]) {
        assert_eq!(pixels.len(), self.width * 4);
        assert!(self.row < self.height);
        let stride = self.rows.len() / 4;
        let offset = self.row % 4 * stride;
        let target = &mut self.rows[offset..offset + stride];
        target[..pixels.len()].copy_from_slice(pixels);
        for edge in target[pixels.len()..].chunks_exact_mut(4) {
            edge.copy_from_slice(&pixels[pixels.len() - 4..]);
        }
        self.row += 1;
        if self.row % 4 != 0 && self.row != self.height {
            return;
        }
        // Clamp the bottom edge just as the full-atlas encoder did.
        if self.row % 4 != 0 {
            for y in self.row % 4..4 {
                self.rows.copy_within(offset..offset + stride, y * stride);
            }
        }
        let surface = RgbaSurface {
            data: &self.rows,
            width: (stride / 4) as u32,
            height: 4,
            stride: stride as u32,
        };
        let bytes = self.width.div_ceil(4) * 16;
        let offset = (self.row - 1) / 4 * bytes;
        bc7::compress_blocks_into(
            &self.settings,
            &surface,
            &mut self.blocks[offset..offset + bytes],
        );
    }

    pub(super) fn finish(self) -> Vec<u8> {
        assert_eq!(self.row, self.height);
        self.blocks
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn large_atlas_only_allocates_blocks_and_four_rgba_rows() {
        let encoder = Encoder::new(4096, 4096);
        assert_eq!(encoder.blocks.capacity(), 16 * 1024 * 1024);
        assert_eq!(encoder.rows.capacity(), 64 * 1024);
    }

    #[test]
    fn row_encoding_matches_full_surface_with_clamped_edges() {
        for (width, height) in [(1usize, 1usize), (1, 7), (5, 3), (7, 9), (8, 8)] {
            let stride = width.div_ceil(4) * 16;
            let padded_height = height.div_ceil(4) * 4;
            let pixels: Vec<u8> = (0..width * height * 4)
                .map(|i| ((i * 73 + i / 7) % 256) as u8)
                .collect();
            let mut padded = vec![0; stride * padded_height];
            for y in 0..padded_height {
                for x in 0..stride / 4 {
                    let source = (y.min(height - 1) * width + x.min(width - 1)) * 4;
                    let target = y * stride + x * 4;
                    padded[target..target + 4].copy_from_slice(&pixels[source..source + 4]);
                }
            }
            let expected = bc7::compress_blocks(
                &bc7::alpha_basic_settings(),
                &RgbaSurface {
                    data: &padded,
                    width: (stride / 4) as u32,
                    height: padded_height as u32,
                    stride: stride as u32,
                },
            );
            let mut encoder = Encoder::new(width as u32, height as u32);
            for row in pixels.chunks_exact(width * 4) {
                encoder.push(row);
            }
            assert_eq!(encoder.finish(), expected, "{width}x{height}");
        }
    }
}
