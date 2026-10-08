use super::*;

macro_rules! getter {
    ($name:ident, $result:ty, $fallback:expr, $arg:ident, $body:expr) => {
        #[no_mangle]
        pub unsafe extern "C" fn $name($arg: *const Node) -> $result {
            crate::guard($fallback, || $body)
        }
    };
}
getter!(
    bongo_json_get_type,
    u32,
    0,
    n,
    match n.as_ref().map(|n| &n.data) {
        Some(Data::Null) => 1,
        Some(Data::Bool(_)) => 2,
        Some(Data::Number(_)) => 3,
        Some(Data::String(..)) => 4,
        Some(Data::Array(_)) => 5,
        Some(Data::Object(_)) => 6,
        None => 0,
    }
);
getter!(
    bongo_json_is_null,
    bool,
    false,
    n,
    matches!(n.as_ref().map(|n| &n.data), Some(Data::Null))
);
getter!(
    bongo_json_is_bool,
    bool,
    false,
    n,
    matches!(n.as_ref().map(|n| &n.data), Some(Data::Bool(_)))
);
getter!(
    bongo_json_is_num,
    bool,
    false,
    n,
    matches!(n.as_ref().map(|n| &n.data), Some(Data::Number(_)))
);
getter!(
    bongo_json_is_str,
    bool,
    false,
    n,
    matches!(n.as_ref().map(|n| &n.data), Some(Data::String(..)))
);
getter!(
    bongo_json_is_arr,
    bool,
    false,
    n,
    matches!(n.as_ref().map(|n| &n.data), Some(Data::Array(_)))
);
getter!(
    bongo_json_is_obj,
    bool,
    false,
    n,
    matches!(n.as_ref().map(|n| &n.data), Some(Data::Object(_)))
);
getter!(
    bongo_json_is_int,
    bool,
    false,
    n,
    matches!(n.as_ref().map(|n|&n.data),Some(Data::Number(v)) if v.is_i64()||v.is_u64())
);
getter!(
    bongo_json_is_uint,
    bool,
    false,
    n,
    matches!(n.as_ref().map(|n|&n.data),Some(Data::Number(v)) if v.is_u64())
);
getter!(
    bongo_json_get_bool,
    bool,
    false,
    n,
    matches!(n.as_ref().map(|n| &n.data), Some(Data::Bool(true)))
);
getter!(bongo_json_is_true, bool, false, n, bongo_json_get_bool(n));
getter!(
    bongo_json_get_num,
    f64,
    0.0,
    n,
    match n.as_ref().map(|n| &n.data) {
        Some(Data::Number(v)) => v.as_f64().unwrap_or(0.0),
        _ => 0.0,
    }
);
getter!(
    bongo_json_get_int,
    i64,
    0,
    n,
    match n.as_ref().map(|n| &n.data) {
        Some(Data::Number(v)) => v.as_i64().unwrap_or_else(|| v.as_u64().unwrap_or(0) as i64),
        _ => 0,
    }
);
getter!(bongo_json_get_sint, i64, 0, n, bongo_json_get_int(n));
getter!(
    bongo_json_get_str,
    *const c_char,
    ptr::null(),
    n,
    match n.as_ref().map(|n| &n.data) {
        Some(Data::String(_, bytes)) => bytes.as_ptr().cast(),
        _ => ptr::null(),
    }
);
getter!(
    bongo_json_mut_get_str,
    *const c_char,
    ptr::null(),
    n,
    bongo_json_get_str(n)
);
getter!(
    bongo_json_get_len,
    usize,
    0,
    n,
    match n.as_ref().map(|n| &n.data) {
        Some(Data::String(v, _)) => v.len(),
        Some(Data::Array(v)) => v.len(),
        Some(Data::Object(v)) => v.len(),
        _ => 0,
    }
);
getter!(
    bongo_json_arr_size,
    usize,
    0,
    n,
    match n.as_ref().map(|n| &n.data) {
        Some(Data::Array(v)) => v.len(),
        _ => 0,
    }
);
getter!(
    bongo_json_obj_size,
    usize,
    0,
    n,
    match n.as_ref().map(|n| &n.data) {
        Some(Data::Object(v)) => v.len(),
        _ => 0,
    }
);
getter!(bongo_json_mut_arr_size, usize, 0, n, bongo_json_arr_size(n));
getter!(
    bongo_json_arr_get_first,
    *mut Node,
    ptr::null_mut(),
    n,
    bongo_json_arr_get(n, 0)
);

#[no_mangle]
pub unsafe extern "C" fn bongo_json_arr_get(n: *const Node, index: usize) -> *mut Node {
    match n.as_ref().map(|n| &n.data) {
        Some(Data::Array(v)) => v.get(index).copied().unwrap_or(ptr::null_mut()),
        _ => ptr::null_mut(),
    }
}
#[no_mangle]
pub unsafe extern "C" fn bongo_json_mut_arr_get(n: *const Node, index: usize) -> *mut Node {
    bongo_json_arr_get(n, index)
}
#[no_mangle]
pub unsafe extern "C" fn bongo_json_obj_get(n: *const Node, key: *const c_char) -> *mut Node {
    crate::guard(ptr::null_mut(), || {
        let Some(key) = text(key) else {
            return ptr::null_mut();
        };
        match n.as_ref().map(|n| &n.data) {
            Some(Data::Object(v)) => v
                .iter()
                .find(|(k, _)| matches!(&(**k).data,Data::String(v,_) if v==key))
                .map(|(_, v)| *v)
                .unwrap_or(ptr::null_mut()),
            _ => ptr::null_mut(),
        }
    })
}
#[no_mangle]
pub unsafe extern "C" fn bongo_json_mut_obj_get(n: *const Node, key: *const c_char) -> *mut Node {
    bongo_json_obj_get(n, key)
}
#[no_mangle]
pub unsafe extern "C" fn bongo_json_obj_key_at(n: *const Node, index: usize) -> *mut Node {
    match n.as_ref().map(|n| &n.data) {
        Some(Data::Object(v)) => v.get(index).map(|v| v.0).unwrap_or(ptr::null_mut()),
        _ => ptr::null_mut(),
    }
}
#[no_mangle]
pub unsafe extern "C" fn bongo_json_obj_value_at(n: *const Node, index: usize) -> *mut Node {
    match n.as_ref().map(|n| &n.data) {
        Some(Data::Object(v)) => v.get(index).map(|v| v.1).unwrap_or(ptr::null_mut()),
        _ => ptr::null_mut(),
    }
}
