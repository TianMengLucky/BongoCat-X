use super::*;
use std::ffi::CString;
struct Consumer {
    width: i32,
    height: i32,
    next: i32,
    calls: usize,
    stop: bool,
    bytes: Vec<u8>,
}
unsafe extern "C" fn consume(
    data: *mut c_void,
    pixels: *mut u8,
    width: i32,
    rows: i32,
    height: i32,
    y: i32,
) -> bool {
    let state = &mut *data.cast::<Consumer>();
    assert_eq!((width, height, y), (state.width, state.height, state.next));
    assert!((1..=64).contains(&rows));
    let pixels = std::slice::from_raw_parts_mut(pixels, width as usize * rows as usize * 4);
    state.bytes.extend_from_slice(pixels);
    pixels.fill(0xa5);
    state.next += rows;
    state.calls += 1;
    !state.stop
}
fn fixture(path: &Path, width: u32, height: u32, pixels: &[u8]) {
    let mut encoder = png::Encoder::new(File::create(path).unwrap(), width, height);
    encoder.set_color(png::ColorType::Rgba);
    encoder.set_depth(png::BitDepth::Eight);
    let mut writer = encoder.write_header().unwrap();
    writer.write_image_data(pixels).unwrap();
    writer.finish().unwrap();
}
fn run(path: &Path, width: i32, height: i32, stop: bool) -> (i32, Consumer) {
    let mut state = Consumer {
        width,
        height,
        next: 0,
        calls: 0,
        stop,
        bytes: Vec::new(),
    };
    let callbacks = Callbacks {
        rows: Some(consume),
        consumer: (&mut state as *mut Consumer).cast(),
        progress: None,
        userdata: std::ptr::null_mut(),
        cancelled: None,
        cancel_data: std::ptr::null_mut(),
    };
    let path = CString::new(path.to_str().unwrap()).unwrap();
    let result = unsafe { bongo_safe_png_rows(path.as_ptr(), width, height, callbacks) };
    (result, state)
}
#[test]
fn mutable_output_strips_do_not_corrupt_later_prediction_rows() {
    let directory = tempfile::tempdir().unwrap();
    let path = directory.path().join("atlas.png");
    let pixels: Vec<u8> = (0..193 * 257 * 4)
        .map(|i| (i * 37 + i / 251) as u8)
        .collect();
    fixture(&path, 193, 257, &pixels);
    let (status, state) = run(&path, 193, 257, false);
    assert_eq!(status, 0);
    assert_eq!(state.next, 257);
    assert!(state.calls > 1);
    assert_eq!(state.bytes, pixels);
    let (status, state) = run(&path, 193, 257, true);
    assert_eq!(status, CANCELLED);
    assert_eq!(state.calls, 1);

    // Exercise the RGB conversion path with the same mutable strip consumer.
    let rgb: Vec<u8> = pixels
        .chunks_exact(4)
        .flat_map(|p| p[..3].iter().copied())
        .collect();
    let mut encoder = png::Encoder::new(File::create(&path).unwrap(), 193, 257);
    encoder.set_color(png::ColorType::Rgb);
    encoder.set_depth(png::BitDepth::Eight);
    let mut writer = encoder.write_header().unwrap();
    writer.write_image_data(&rgb).unwrap();
    writer.finish().unwrap();
    let (status, state) = run(&path, 193, 257, false);
    assert_eq!(status, 0);
    let mut expected = pixels;
    for pixel in expected.chunks_exact_mut(4) {
        pixel[3] = 255;
    }
    assert_eq!(state.bytes, expected);
}
#[test]
fn area_filter_ignores_invisible_rgb_and_keeps_straight_alpha() {
    let directory = tempfile::tempdir().unwrap();
    let path = directory.path().join("edge.png");
    fixture(
        &path,
        2,
        2,
        &[255, 0, 0, 0, 0, 100, 0, 255, 0, 100, 0, 255, 0, 100, 0, 255],
    );
    let (status, state) = run(&path, 1, 1, false);
    assert_eq!(status, 0);
    assert_eq!(state.bytes, [0, 100, 0, 191]);
}
