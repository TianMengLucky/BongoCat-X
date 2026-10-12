use crate::guarded;
use std::ffi::c_void;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Page {
    pub struct_size: u32,
    pub id: [u8; 32],
    pub title_key: [u8; 128],
    pub title: [u8; 64],
    pub icon: u32,
}

impl Default for Page {
    fn default() -> Self {
        Self {
            struct_size: std::mem::size_of::<Self>() as u32,
            id: [0; 32],
            title_key: [0; 128],
            title: [0; 64],
            icon: 1,
        }
    }
}

unsafe extern "C" fn page(instance: *mut c_void, output: *mut Page) -> u32 {
    guarded(0, || {
        if instance.is_null()
            || output.is_null()
            || (*output).struct_size as usize != std::mem::size_of::<Page>()
        {
            return 0;
        }
        let mut contribution = Page::default();
        contribution.id[..10].copy_from_slice(b"statistics");
        let key = b"pages.preference.statistics.title";
        contribution.title_key[..key.len()].copy_from_slice(key);
        contribution.title[..10].copy_from_slice(b"Statistics");
        *output = contribution;
        1
    })
}

#[repr(C)]
pub struct Plugin {
    abi_version: u32,
    struct_size: u32,
    page: unsafe extern "C" fn(*mut c_void, *mut Page) -> u32,
}

static PLUGIN: Plugin = Plugin {
    abi_version: 1,
    struct_size: std::mem::size_of::<Plugin>() as u32,
    page,
};

#[no_mangle]
pub extern "C" fn bongo_cat_statistics_page_query(abi: u32) -> *const Plugin {
    if abi == 1 {
        &PLUGIN
    } else {
        std::ptr::null()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn navigation_contribution_requires_active_instance_and_compatible_layout() {
        assert!(bongo_cat_statistics_page_query(0).is_null());
        assert!(bongo_cat_statistics_page_query(2).is_null());
        assert!(!bongo_cat_statistics_page_query(1).is_null());
        assert_eq!(std::mem::size_of::<Page>(), 232);
        let mut output = Page::default();
        assert_eq!(unsafe { page(std::ptr::null_mut(), &mut output) }, 0);
        let mut instance = 0u8;
        let instance = (&mut instance as *mut u8).cast();
        assert_eq!(unsafe { page(instance, std::ptr::null_mut()) }, 0);
        output.struct_size = 0;
        assert_eq!(unsafe { page(instance, &mut output) }, 0);
        output = Page::default();
        assert_eq!(unsafe { page(instance, &mut output) }, 1);
        assert_eq!(&output.id[..11], b"statistics\0");
        assert_eq!(&output.title[..11], b"Statistics\0");
        assert_eq!(output.icon, 1);
        #[cfg(target_pointer_width = "64")]
        assert_eq!(std::mem::size_of::<Plugin>(), 16);
    }
}
