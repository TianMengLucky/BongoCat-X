use super::*;
use serde::{
    ser::{SerializeMap, SerializeSeq},
    Serialize, Serializer,
};
use std::ffi::{c_void, CString};

// The C caller keeps the document live and immutable throughout serialization.
struct JsonNode<'a> {
    node: Option<&'a Node>,
    depth: usize,
}
impl Serialize for JsonNode<'_> {
    fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
        if self.depth > 128 {
            return Err(serde::ser::Error::custom("JSON nesting exceeds 128 levels"));
        }
        match self.node.map(|node| &node.data) {
            None | Some(Data::Null) => serializer.serialize_unit(),
            Some(Data::Bool(value)) => serializer.serialize_bool(*value),
            Some(Data::Number(value)) => value.serialize(serializer),
            Some(Data::String(value)) => serializer.serialize_str(value.text()),
            Some(Data::Array(values)) => {
                let mut sequence = serializer.serialize_seq(Some(values.len()))?;
                for &value in values {
                    // SAFETY: document nodes stay live and immutable during serialization.
                    sequence.serialize_element(&JsonNode {
                        node: unsafe { value.as_ref() },
                        depth: self.depth + 1,
                    })?;
                }
                sequence.end()
            }
            Some(Data::Object(values)) => {
                let mut map = serializer.serialize_map(None)?;
                for &(key, value) in values {
                    // SAFETY: keys and values belong to live, immutable document nodes.
                    if let Some(Node {
                        data: Data::String(key),
                    }) = unsafe { key.as_ref() }
                    {
                        map.serialize_entry(
                            key.text(),
                            &JsonNode {
                                // SAFETY: the value follows the same document lifetime as its key.
                                node: unsafe { value.as_ref() },
                                depth: self.depth + 1,
                            },
                        )?;
                    }
                }
                map.end()
            }
        }
    }
}

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
        add_object(
            doc,
            obj,
            key,
            Data::String(JsonString::new(value.to_owned())),
        )
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
        items.retain(|(k, _)| !matches!(&(**k).data,Data::String(v) if v.text()==key));
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
/// # Safety
/// The document and reachable nodes must stay live and immutable for this call.
/// `length`, when non-NULL, must be writable.
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
        let value = JsonNode {
            node: doc.root.as_ref(),
            depth: 0,
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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn serializes_borrowed_nodes_like_owned_values() {
        let value: Value = serde_json::from_str(
            r#"{"empty":{},"array":[null,true,-2,1.5],"name":"a\u0000b","id":18446744073709551615}"#,
        ).unwrap();
        let mut doc = Doc::new();
        doc.root = doc.from_value(value.clone());
        for flags in [0, 1] {
            let mut length = 0;
            let output = unsafe { bongo_json_write(&doc, flags, &mut length) };
            assert!(!output.is_null());
            let expected = if flags == 0 {
                serde_json::to_string(&value).unwrap()
            } else {
                serde_json::to_string_pretty(&value).unwrap()
            };
            assert_eq!(length, expected.len());
            assert_eq!(
                unsafe { std::ffi::CStr::from_ptr(output) }.to_bytes(),
                expected.as_bytes()
            );
            unsafe { bongo_json_free_text(output) };
        }
    }

    #[test]
    fn preserves_the_nesting_limit() {
        for depth in [128, 129] {
            let mut value = Value::Null;
            for _ in 0..depth {
                value = Value::Array(vec![value]);
            }
            let mut doc = Doc::new();
            doc.root = doc.from_value(value);
            let mut length = 0;
            let output = unsafe { bongo_json_write(&doc, 0, &mut length) };
            assert_eq!(!output.is_null(), depth == 128);
            assert_eq!(length, if depth == 128 { 260 } else { 0 });
            unsafe { bongo_json_free_text(output) };
        }
    }

    #[test]
    fn rejects_cycles_without_returning_partial_output() {
        let mut doc = Doc::new();
        doc.root = doc.alloc(Data::Array(Vec::new()));
        let mut length = 123;
        unsafe {
            assert!(bongo_json_mut_arr_add_val(doc.root, doc.root));
            assert!(bongo_json_write(&doc, 0, &mut length).is_null());
        }
        assert_eq!(length, 0);
    }
}
