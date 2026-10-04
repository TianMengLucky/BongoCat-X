//! Audio decoding over the C ABI, replacing the decoders embedded in
//! miniaudio (stb_vorbis / dr_wav / dr_mp3) for user-supplied sound files.
//! miniaudio keeps owning the output device; it receives decoded PCM.

use crate::{guard, release_exact};
use std::ffi::CStr;
use std::io::ErrorKind;
use std::os::raw::c_char;
use symphonia::core::audio::SampleBuffer;
use symphonia::core::codecs::{DecoderOptions, CODEC_TYPE_NULL};
use symphonia::core::errors::Error;
use symphonia::core::formats::FormatOptions;
use symphonia::core::io::MediaSourceStream;
use symphonia::core::meta::MetadataOptions;
use symphonia::core::probe::Hint;

/// Hard cap on decoded PCM (5 minutes of 48 kHz stereo) so a crafted or
/// accidental long file cannot balloon memory across the voice pool the way
/// unrestricted decoding would.
const MAX_SAMPLES: usize = 48_000 * 2 * 300;

/// # Safety
/// `path` must be a NUL-terminated string; the out pointers writable. On
/// success `*samples` holds `frame_count * channels` interleaved f32 frames
/// released with `bongo_safe_free_samples`.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_audio_decode_file(
    path: *const c_char,
    samples: *mut *mut f32,
    frame_count: *mut u64,
    sample_rate: *mut u32,
    channels: *mut u32,
) -> bool {
    guard(false, || {
        if path.is_null()
            || samples.is_null()
            || frame_count.is_null()
            || sample_rate.is_null()
            || channels.is_null()
        {
            return false;
        }
        *samples = std::ptr::null_mut();
        *frame_count = 0;
        *sample_rate = 0;
        *channels = 0;
        let raw = CStr::from_ptr(path).to_bytes();
        let path = match std::str::from_utf8(raw) {
            Ok(text) => std::path::PathBuf::from(text),
            Err(_) => return false,
        };
        let file = match std::fs::File::open(&path) {
            Ok(file) => file,
            Err(_) => return false,
        };
        let mut hint = Hint::new();
        if let Some(extension) = path.extension().and_then(|e| e.to_str()) {
            hint.with_extension(extension);
        }
        let source = MediaSourceStream::new(Box::new(file), Default::default());
        let mut probed = match symphonia::default::get_probe().format(
            &hint,
            source,
            &FormatOptions::default(),
            &MetadataOptions::default(),
        ) {
            Ok(probed) => probed,
            Err(_) => return false,
        };
        let (track_id, codec_params) = match probed
            .format
            .tracks()
            .iter()
            .find(|track| track.codec_params.codec != CODEC_TYPE_NULL)
        {
            Some(track) => (track.id, track.codec_params.clone()),
            None => return false,
        };
        let mut decoder = match symphonia::default::get_codecs()
            .make(&codec_params, &DecoderOptions::default())
        {
            Ok(decoder) => decoder,
            Err(_) => return false,
        };
        let mut decoded_samples: Vec<f32> = Vec::new();
        let mut spec = None;
        loop {
            let packet = match probed.format.next_packet() {
                Ok(packet) => packet,
                Err(Error::IoError(error)) if error.kind() == ErrorKind::UnexpectedEof => break,
                Err(_) => return false,
            };
            if packet.track_id() != track_id {
                continue;
            }
            let buffer = match decoder.decode(&packet) {
                Ok(buffer) => buffer,
                Err(Error::DecodeError(_)) => continue,
                Err(_) => return false,
            };
            let buffer_spec = *buffer.spec();
            match spec {
                None => spec = Some(buffer_spec),
                Some(existing) if existing != buffer_spec => return false,
                _ => {}
            }
            if decoded_samples.len() + buffer.capacity() * buffer_spec.channels.count()
                > MAX_SAMPLES
            {
                return false;
            }
            let mut converted = SampleBuffer::<f32>::new(buffer.capacity() as u64, buffer_spec);
            converted.copy_interleaved_ref(buffer);
            decoded_samples.extend_from_slice(converted.samples());
        }
        let spec = match spec {
            Some(spec) if spec.rate > 0 && spec.channels.count() > 0 => spec,
            _ => return false,
        };
        let channels_count = spec.channels.count();
        if decoded_samples.is_empty() || !decoded_samples.len().is_multiple_of(channels_count) {
            return false;
        }
        *frame_count = (decoded_samples.len() / channels_count) as u64;
        *sample_rate = spec.rate;
        *channels = channels_count as u32;
        *samples = release_exact(decoded_samples);
        true
    })
}

/// # Safety
/// `samples` must come from `bongo_safe_audio_decode_file` with the same
/// sample count and must not have been freed yet.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_free_samples(samples: *mut f32, count: usize) {
    guard((), || {
        if samples.is_null() || count == 0 {
            return;
        }
        drop(crate::reclaim_exact(samples, count));
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    pub(super) fn write_wave(path: &std::path::Path, frames: usize) {
        use std::io::Write;
        let mut file = std::fs::File::create(path).unwrap();
        fn little(file: &mut std::fs::File, value: u32, bytes: usize) {
            for index in 0..bytes {
                file.write_all(&[(value >> (index * 8)) as u8]).unwrap();
            }
        }
        file.write_all(b"RIFF").unwrap();
        little(&mut file, (36 + frames * 2) as u32, 4);
        file.write_all(b"WAVEfmt ").unwrap();
        little(&mut file, 16, 4);
        little(&mut file, 1, 2); // PCM
        little(&mut file, 1, 2); // mono
        little(&mut file, 8000, 4);
        little(&mut file, 8000 * 2, 4);
        little(&mut file, 2, 2);
        little(&mut file, 16, 2);
        file.write_all(b"data").unwrap();
        little(&mut file, (frames * 2) as u32, 4);
        for index in 0..frames {
            little(&mut file, if (index / 16) % 2 == 0 { 8000 } else { 0 }, 2);
        }
    }

    #[test]
    fn decodes_wav() {
        let directory = std::env::temp_dir().join("bongo-safe-test");
        std::fs::create_dir_all(&directory).unwrap();
        let path = directory.join("test.wav");
        write_wave(&path, 800);
        // C callers pass NUL-terminated strings; mirror that here.
        let path_cstring = std::ffi::CString::new(path.to_str().unwrap()).unwrap();
        let mut samples: *mut f32 = std::ptr::null_mut();
        let mut frames: u64 = 0;
        let mut rate: u32 = 0;
        let mut channels: u32 = 0;
        let ok = unsafe {
            bongo_safe_audio_decode_file(
                path_cstring.as_ptr(),
                &mut samples,
                &mut frames,
                &mut rate,
                &mut channels,
            )
        };
        assert!(ok);
        assert_eq!(frames, 800);
        assert_eq!(rate, 8000);
        assert_eq!(channels, 1);
        let slice = unsafe { std::slice::from_raw_parts(samples, frames as usize) };
        assert!(slice[0].abs() > 0.1);
        unsafe { bongo_safe_free_samples(samples, frames as usize) };
        let _ = std::fs::remove_file(&path);
    }

    #[test]
    fn rejects_missing_files() {
        let mut samples: *mut f32 = std::ptr::null_mut();
        let mut frames: u64 = 0;
        let mut rate: u32 = 0;
        let mut channels: u32 = 0;
        let missing = std::ffi::CString::new("definitely/missing/sound.wav").unwrap();
        let ok = unsafe {
            bongo_safe_audio_decode_file(
                missing.as_ptr(),
                &mut samples,
                &mut frames,
                &mut rate,
                &mut channels,
            )
        };
        assert!(!ok);
        assert!(samples.is_null());
    }
}
