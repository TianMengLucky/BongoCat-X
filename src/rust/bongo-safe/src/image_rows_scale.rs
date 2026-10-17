use super::{Callbacks, Consumer, Outcome, FORMAT, STRIP_BYTES};
pub(super) struct Scaler {
    sw: u32,
    sh: u32,
    w: u32,
    h: u32,
    y: u32,
    buffered: u32,
    batch: u32,
    horizontal: Vec<f64>,
    sum: Vec<f64>,
    pixels: Vec<u8>,
}
impl Scaler {
    pub(super) fn new(sw: u32, sh: u32, bounds: (u32, u32)) -> Result<Self, i32> {
        let (w, h) = crate::native_texture::fitted(sw, sh, bounds.0, bounds.1);
        let stride = w as usize * 4;
        let filter_len = if (sw, sh) == (w, h) { 0 } else { stride };
        let batch = (STRIP_BYTES / stride).clamp(1, 64).min(h as usize) as u32;
        Ok(Self {
            sw,
            sh,
            w,
            h,
            y: 0,
            buffered: 0,
            batch,
            horizontal: vec![0.0; filter_len],
            sum: vec![0.0; filter_len],
            pixels: vec![0; stride * batch as usize],
        })
    }
    fn flush(&mut self, callbacks: Callbacks, consume: &mut Consumer<'_>) -> Outcome {
        if self.buffered == 0 {
            return Ok(());
        }
        callbacks.check()?;
        let bytes = self.w as usize * self.buffered as usize * 4;
        consume(
            &mut self.pixels[..bytes],
            self.w,
            self.buffered,
            self.h,
            self.y - self.buffered,
        )?;
        self.buffered = 0;
        Ok(())
    }
    pub(super) fn push(
        &mut self,
        rgba: &[u8],
        sy: u32,
        callbacks: Callbacks,
        consume: &mut Consumer<'_>,
    ) -> Outcome {
        let stride = self.w as usize * 4;
        if (self.sw, self.sh) == (self.w, self.h) {
            let offset = self.buffered as usize * stride;
            self.pixels[offset..offset + stride].copy_from_slice(rgba);
            self.y += 1;
            self.buffered += 1;
            if self.buffered == self.batch {
                self.flush(callbacks, consume)?;
            }
            return Ok(());
        }
        self.horizontal.fill(0.0);
        let sx = f64::from(self.sw) / f64::from(self.w);
        for x in 0..self.w as usize {
            let left = x as f64 * sx;
            let right = (x + 1) as f64 * sx;
            for ix in left.floor() as usize..(right.ceil() as usize).min(self.sw as usize) {
                let weight = right.min((ix + 1) as f64) - left.max(ix as f64);
                let p = &rgba[ix * 4..ix * 4 + 4];
                let alpha = f64::from(p[3]) * weight;
                for (c, value) in p[..3].iter().enumerate() {
                    self.horizontal[x * 4 + c] += f64::from(*value) * alpha;
                }
                self.horizontal[x * 4 + 3] += alpha;
            }
        }
        let scale_y = f64::from(self.sh) / f64::from(self.h);
        let mut top = f64::from(sy);
        let bottom = f64::from(sy + 1);
        while top < bottom && self.y < self.h {
            callbacks.check()?;
            let edge = (f64::from(self.y + 1) * scale_y).min(bottom);
            for (sum, v) in self.sum.iter_mut().zip(&self.horizontal) {
                *sum += v * (edge - top);
            }
            top = edge;
            if edge >= f64::from(self.y + 1) * scale_y || sy + 1 == self.sh {
                let offset = self.buffered as usize * stride;
                for (target, values) in self.pixels[offset..offset + stride]
                    .chunks_exact_mut(4)
                    .zip(self.sum.chunks_exact(4))
                {
                    target[3] = (values[3] / (sx * scale_y)).round().clamp(0.0, 255.0) as u8;
                    for c in 0..3 {
                        target[c] = if target[3] == 0 {
                            0
                        } else {
                            (values[c] / values[3]).round().clamp(0.0, 255.0) as u8
                        };
                    }
                }
                self.sum.fill(0.0);
                self.y += 1;
                self.buffered += 1;
                if self.buffered == self.batch {
                    self.flush(callbacks, consume)?;
                }
            }
        }
        Ok(())
    }
    pub(super) fn finish(mut self, callbacks: Callbacks, consume: &mut Consumer<'_>) -> Outcome {
        if self.y != self.h {
            return Err(FORMAT);
        }
        self.flush(callbacks, consume)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn original_size_does_not_allocate_filter_rows() {
        let scaler = Scaler::new(4096, 4096, (4096, 4096)).unwrap();
        assert_eq!(scaler.horizontal.capacity(), 0);
        assert_eq!(scaler.sum.capacity(), 0);
        let scaled = Scaler::new(4096, 4096, (2048, 2048)).unwrap();
        assert_eq!(scaled.horizontal.len(), 2048 * 4);
        assert_eq!(scaled.sum.len(), 2048 * 4);
    }
}
