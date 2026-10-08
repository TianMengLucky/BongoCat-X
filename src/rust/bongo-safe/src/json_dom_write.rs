use super::*;
use std::ffi::{c_void, CString};
#[no_mangle]
pub unsafe extern "C" fn bongo_json_mut_doc_new(_: *const c_void) -> *mut Doc {
    crate::guard(ptr::null_mut(), || Box::into_raw(Box::new(Doc::new())))
}
#[no_mangle]
pub unsafe extern "C" fn bongo_json_mut_doc_free(doc: *mut Doc) {
    read::bongo_json_doc_free(doc);
}
#[no_mangle]
pub unsafe extern "C" fn bongo_json_mut_doc_set_root(doc: *mut Doc, node: *mut Node) {
    if let Some(doc) = doc.as_mut() {
        doc.root = node;
    }
}
#[no_mangle]
pub unsafe extern "C" fn bongo_json_mut_obj(doc: *mut Doc) -> *mut Node {
    crate::guard(ptr::null_mut(), || {
        doc.as_mut()
            .map(|d| d.alloc(Data::Object(vec![])))
            .unwrap_or(ptr::null_mut())
    })
}
#[no_mangle]
pub unsafe extern "C" fn bongo_json_mut_arr(doc: *mut Doc) -> *mut Node {
    crate::guard(ptr::null_mut(), || {
        doc.as_mut()
            .map(|d| d.alloc(Data::Array(vec![])))
            .unwrap_or(ptr::null_mut())
    })
}
macro_rules! obj_add {
    ($name:ident,$t:ty,$convert:expr) => {
        #[no_mangle]
        pub unsafe extern "C" fn $name(
            doc: *mut Doc,
            obj: *mut Node,
            key: *const c_char,
            value: $t,
        ) -> bool {
            crate::guard(false, || add_object(doc, obj, key, ($convert)(value)))
        }
    };
}
obj_add!(bongo_json_mut_obj_add_bool, bool, Data::Bool);
obj_add!(bongo_json_mut_obj_add_int, i64, |v| Data::Number(
    Number::from(v)
));
obj_add!(bongo_json_mut_obj_add_uint, u64, |v| Data::Number(
    Number::from(v)
));
#[no_mangle]
pub unsafe extern "C" fn bongo_json_mut_obj_add_real(
    doc: *mut Doc,
    obj: *mut Node,
    key: *const c_char,
    value: f64,
) -> bool {
    crate::guard(false, || {
        Number::from_f64(value).is_some_and(|v| add_object(doc, obj, key, Data::Number(v)))
    })
}
#[no_mangle]
pub unsafe extern "C" fn bongo_json_mut_obj_add_str(
    doc: *mut Doc,
    obj: *mut Node,
    key: *const c_char,
    value: *const c_char,
) -> bool {
    crate::guard(false, || {
        let Some(value) = text(value) else {
            return false;
        };
        let mut bytes = value.as_bytes().to_vec();
        bytes.push(0);
        add_object(doc, obj, key, Data::String(value.to_owned(), bytes))
    })
}
#[no_mangle]
pub unsafe extern "C" fn bongo_json_mut_obj_add_strcpy(
    doc: *mut Doc,
    obj: *mut Node,
    key: *const c_char,
    value: *const c_char,
) -> bool {
    bongo_json_mut_obj_add_str(doc, obj, key, value)
}
macro_rules! obj_container {
    ($name:ident,$data:expr) => {
        #[no_mangle]
        pub unsafe extern "C" fn $name(
            doc: *mut Doc,
            obj: *mut Node,
            key: *const c_char,
        ) -> *mut Node {
            crate::guard(ptr::null_mut(), || {
                if add_object(doc, obj, key, $data) {
                    access::bongo_json_obj_get(obj, key)
                } else {
                    ptr::null_mut()
                }
            })
        }
    };
}
obj_container!(bongo_json_mut_obj_add_obj, Data::Object(vec![]));
obj_container!(bongo_json_mut_obj_add_arr, Data::Array(vec![]));
#[no_mangle]
pub unsafe extern "C" fn bongo_json_mut_obj_remove_key(obj: *mut Node, key: *const c_char) -> bool {
    crate::guard(false, || {
        let (Some(obj), Some(key)) = (obj.as_mut(), text(key)) else {
            return false;
        };
        let Data::Object(items) = &mut obj.data else {
            return false;
        };
        let before = items.len();
        items.retain(|(k, _)| !matches!(&(**k).data,Data::String(v,_) if v==key));
        before != items.len()
    })
}
#[no_mangle]
pub unsafe extern "C" fn bongo_json_mut_arr_add_val(arr: *mut Node, value: *mut Node) -> bool {
    crate::guard(false, || {
        if let Some(Node {
            data: Data::Array(items),
        }) = arr.as_mut()
        {
            if !value.is_null() {
                items.push(value);
                return true;
            }
        }
        false
    })
}
macro_rules! arr_container {
    ($name:ident,$make:ident) => {
        #[no_mangle]
        pub unsafe extern "C" fn $name(doc: *mut Doc, arr: *mut Node) -> *mut Node {
            crate::guard(ptr::null_mut(), || {
                let value = $make(doc);
                if bongo_json_mut_arr_add_val(arr, value) {
                    value
                } else {
                    ptr::null_mut()
                }
            })
        }
    };
}
arr_container!(bongo_json_mut_arr_add_obj, bongo_json_mut_obj);
arr_container!(bongo_json_mut_arr_add_arr, bongo_json_mut_arr);
#[no_mangle]
pub unsafe extern "C" fn bongo_json_mut_arr_add_int(
    doc: *mut Doc,
    arr: *mut Node,
    value: i64,
) -> bool {
    crate::guard(false, || {
        doc.as_mut().is_some_and(|doc| {
            bongo_json_mut_arr_add_val(arr, doc.alloc(Data::Number(value.into())))
        })
    })
}
#[no_mangle]
pub unsafe extern "C" fn bongo_json_mut_arr_add_real(
    doc: *mut Doc,
    arr: *mut Node,
    value: f64,
) -> bool {
    crate::guard(false, || {
        if let (Some(doc), Some(value)) = (doc.as_mut(), Number::from_f64(value)) {
            return bongo_json_mut_arr_add_val(arr, doc.alloc(Data::Number(value)));
        }
        false
    })
}
#[no_mangle]
pub unsafe extern "C" fn bongo_json_write(
    doc: *const Doc,
    flags: u32,
    length: *mut usize,
) -> *mut c_char {
    crate::guard(ptr::null_mut(), || {
        if let Some(length) = length.as_mut() {
            *length = 0;
        }
        let Some(doc) = doc.as_ref() else {
            return ptr::null_mut();
        };
        let Ok(value) = to_value(doc.root, 0) else {
            return ptr::null_mut();
        };
        let result = if flags & 1 != 0 {
            serde_json::to_string_pretty(&value)
        } else {
            serde_json::to_string(&value)
        };
        let Ok(result) = result else {
            return ptr::null_mut();
        };
        if let Some(length) = length.as_mut() {
            *length = result.len();
        }
        CString::new(result)
            .map(CString::into_raw)
            .unwrap_or(ptr::null_mut())
    })
}
#[no_mangle]
pub unsafe extern "C" fn bongo_json_free_text(text: *mut c_char) {
    if !text.is_null() {
        drop(CString::from_raw(text));
    }
}
