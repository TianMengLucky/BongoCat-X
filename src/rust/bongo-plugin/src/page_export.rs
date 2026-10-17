/// Exports an isolated UI session; no pointer to plugin worker state is used.
#[macro_export]
macro_rules! export_page {
    ($symbol:ident, $state:ty) => {
        extern "C" fn create_page() -> *mut std::ffi::c_void {
            $crate::guard::guard(std::ptr::null_mut(), || {
                Box::into_raw(Box::new(<$state>::default())).cast()
            })
        }
        unsafe extern "C" fn destroy_page(state: *mut std::ffi::c_void) {
            if !state.is_null() {
                $crate::guard::guard((), || drop(Box::from_raw(state.cast::<$state>())));
            }
        }
        unsafe extern "C" fn draw_page(
            state: *mut std::ffi::c_void,
            ui: *mut $crate::ui::Ui,
            host: *const $crate::page::Host,
            services: *const $crate::page::Services,
        ) -> u32 {
            $crate::guard::guard(0, || {
                if state.is_null() {
                    return 0;
                }
                let Some(page) = $crate::page::Page::new(ui, host, services) else {
                    return 0;
                };
                $crate::page::PageState::draw(&mut *state.cast::<$state>(), &page);
                1
            })
        }
        static PAGE: $crate::page::Contribution = $crate::page::Contribution {
            abi_version: $crate::page::ABI,
            struct_size: std::mem::size_of::<$crate::page::Contribution>() as u32,
            create: create_page,
            destroy: destroy_page,
            draw: draw_page,
        };
        #[no_mangle]
        pub extern "C" fn $symbol(abi: u32) -> *const $crate::page::Contribution {
            if abi == $crate::page::ABI {
                &PAGE
            } else {
                std::ptr::null()
            }
        }
    };
}

#[cfg(test)]
mod tests {
    use crate::page::{Page, PageState};
    use std::sync::atomic::{AtomicUsize, Ordering};
    static LIVE: AtomicUsize = AtomicUsize::new(0);
    struct State {
        _marker: u8,
    }
    impl Default for State {
        fn default() -> Self {
            LIVE.fetch_add(1, Ordering::SeqCst);
            Self { _marker: 0 }
        }
    }
    impl Drop for State {
        fn drop(&mut self) {
            LIVE.fetch_sub(1, Ordering::SeqCst);
        }
    }
    impl PageState for State {
        fn draw(&mut self, _page: &Page<'_>) {
            panic!("invalid host must be rejected before drawing");
        }
    }
    crate::export_page!(test_page_query, State);
    #[test]
    fn sessions_are_isolated_and_invalid_draws_can_be_destroyed() {
        assert!(test_page_query(0).is_null());
        let api = unsafe { &*test_page_query(1) };
        let first = (api.create)();
        let second = (api.create)();
        assert!(!first.is_null() && !second.is_null() && first != second);
        assert_eq!(LIVE.load(Ordering::SeqCst), 2);
        unsafe {
            assert_eq!(
                (api.draw)(
                    first,
                    std::ptr::null_mut(),
                    std::ptr::null(),
                    std::ptr::null()
                ),
                0
            );
            (api.destroy)(first);
            (api.destroy)(std::ptr::null_mut());
            assert_eq!(LIVE.load(Ordering::SeqCst), 1);
            (api.destroy)(second);
        }
        assert_eq!(LIVE.load(Ordering::SeqCst), 0);
    }
}
