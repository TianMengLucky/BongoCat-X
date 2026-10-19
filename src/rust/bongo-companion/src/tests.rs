use super::*;
unsafe extern "C" fn post(
    _: *const c_char,
    _: *const c_char,
    _: *const c_char,
    _: Option<Cancelled>,
    _: *mut c_void,
    _: *mut *mut u8,
    _: *mut usize,
) -> c_int {
    2
}
unsafe extern "C" fn free(_: *mut u8, _: usize) {}
unsafe extern "C" fn replace(_: *const c_char, _: *const c_char) -> u32 {
    0
}
fn instance() -> Box<Instance> {
    let root = std::ffi::CString::new(
        std::env::temp_dir()
            .join("bongo-companion-unit-test")
            .to_string_lossy()
            .as_bytes(),
    )
    .unwrap();
    let host = Host {
        abi_version: 1,
        struct_size: size_of::<Host>() as u32,
        post,
        free_response: free,
        replace,
    };
    let ptr = unsafe { create(root.as_ptr(), &host) };
    assert!(!ptr.is_null());
    unsafe { Box::from_raw(ptr.cast()) }
}
#[test]
fn abi_and_null_arguments() {
    let p = size_of::<usize>();
    assert_eq!(size_of::<Host>(), 8 + 3 * p);
    assert_eq!(size_of::<Api>(), 8 + 5 * p);
    assert_eq!(size_of::<Snapshot>(), 20 + 4096);
    assert!(bongo_cat_companion_query_v1(2).is_null());
    unsafe {
        assert!(create(std::ptr::null(), std::ptr::null()).is_null());
        assert_eq!(snapshot(std::ptr::null_mut(), std::ptr::null_mut()), 0);
        event(std::ptr::null_mut(), 0, 0);
        destroy(std::ptr::null_mut());
    }
}
#[test]
fn disabled_cooldown_idle_and_stale_completion() {
    let mut v = instance();
    v.trigger(1, 100);
    assert!(v.worker.is_none());
    assert!(v.last_request.is_none());
    v.config.enabled = 1;
    v.config.triggers = [0, 1, 1, 1, 0];
    v.last_request = Some(100);
    v.trigger(2, 101);
    assert!(v.worker.is_none());
    assert_eq!(v.last_active, 101);
    // Pet activity must not keep an idle user classified as active.
    v.trigger(4, 1000);
    assert_eq!(v.last_active, 101);
    let cancel = Arc::new(AtomicBool::new(true));
    v.worker = Some(Worker {
        cancel,
        handle: std::thread::spawn(|| Ok("stale reply".into())),
    });
    while !v.worker.as_ref().unwrap().handle.is_finished() {
        std::thread::yield_now();
    }
    v.poll(1001);
    assert!(v.text.is_empty());
    assert_eq!(v.sequence, 0);
    v.text = "reply".into();
    v.expires = 2000;
    v.poll(2000);
    assert!(v.text.is_empty());
}
#[test]
fn destroy_cancels_and_joins_before_unload() {
    let mut v = instance();
    let cancel = Arc::new(AtomicBool::new(false));
    let token = cancel.clone();
    let done = Arc::new(AtomicBool::new(false));
    let completed = done.clone();
    v.worker = Some(Worker {
        cancel,
        handle: std::thread::spawn(move || {
            while !token.load(Ordering::Acquire) {
                std::thread::yield_now();
            }
            completed.store(true, Ordering::Release);
            Err(1)
        }),
    });
    unsafe {
        destroy(Box::into_raw(v).cast());
    }
    assert!(done.load(Ordering::Acquire));
}
