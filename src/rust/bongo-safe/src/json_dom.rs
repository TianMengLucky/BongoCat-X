//! Rust-owned JSON DOM. C only borrows stable node handles until document free.
use serde_json::{Map, Number, Value};
use std::{ffi::c_char, ptr};

#[path = "json_dom_access.rs"]
mod access;
#[path = "json_dom_read.rs"]
pub(crate) mod read;
#[path = "json_dom_write.rs"]
mod write;

pub(crate) enum Data {
    Null,
    Bool(bool),
    Number(Number),
    String(String, Vec<u8>),
    Array(Vec<*mut Node>),
    Object(Vec<(*mut Node, *mut Node)>),
}
pub struct Node {
    pub(crate) data: Data,
}
pub struct Doc {
    pub(crate) nodes: Vec<Box<Node>>,
    pub(crate) root: *mut Node,
}
impl Doc {
    fn new() -> Self {
        Self {
            nodes: Vec::new(),
            root: ptr::null_mut(),
        }
    }
    fn alloc(&mut self, data: Data) -> *mut Node {
        let mut node = Box::new(Node { data });
        let pointer = &mut *node as *mut Node;
        self.nodes.push(node);
        pointer
    }
    fn string(&mut self, text: &str) -> *mut Node {
        let mut bytes = text.as_bytes().to_vec();
        bytes.push(0);
        self.alloc(Data::String(text.to_owned(), bytes))
    }
    fn from_value(&mut self, value: Value) -> *mut Node {
        let data = match value {
            Value::Null => Data::Null,
            Value::Bool(v) => Data::Bool(v),
            Value::Number(v) => Data::Number(v),
            Value::String(v) => return self.string(&v),
            Value::Array(v) => Data::Array(v.into_iter().map(|v| self.from_value(v)).collect()),
            Value::Object(v) => Data::Object(
                v.into_iter()
                    .map(|(k, v)| (self.string(&k), self.from_value(v)))
                    .collect(),
            ),
        };
        self.alloc(data)
    }
}
pub(crate) unsafe fn to_value(node: *const Node, depth: usize) -> Result<Value, String> {
    if depth > 128 {
        return Err("JSON nesting exceeds 128 levels".into());
    }
    Ok(match node.as_ref().map(|v| &v.data) {
        None | Some(Data::Null) => Value::Null,
        Some(Data::Bool(v)) => Value::Bool(*v),
        Some(Data::Number(v)) => Value::Number(v.clone()),
        Some(Data::String(v, _)) => Value::String(v.clone()),
        Some(Data::Array(v)) => Value::Array(
            v.iter()
                .map(|v| to_value(*v, depth + 1))
                .collect::<Result<_, _>>()?,
        ),
        Some(Data::Object(v)) => {
            let mut result = Map::new();
            for &(key, value) in v {
                if let Data::String(key, _) = &(*key).data {
                    result.insert(key.clone(), to_value(value, depth + 1)?);
                }
            }
            Value::Object(result)
        }
    })
}
unsafe fn text<'a>(value: *const c_char) -> Option<&'a str> {
    if value.is_null() {
        None
    } else {
        std::ffi::CStr::from_ptr(value).to_str().ok()
    }
}
unsafe fn add_object(doc: *mut Doc, obj: *mut Node, key: *const c_char, data: Data) -> bool {
    let (Some(doc), Some(obj), Some(key)) = (doc.as_mut(), obj.as_mut(), text(key)) else {
        return false;
    };
    let Data::Object(items) = &mut obj.data else {
        return false;
    };
    let value = doc.alloc(data);
    if let Some(pair) = items
        .iter_mut()
        .find(|(k, _)| matches!(&(**k).data,Data::String(v,_) if v==key))
    {
        pair.1 = value;
    } else {
        items.push((doc.string(key), value));
    }
    true
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn compatible_json_bounds_nesting_and_repairs_mver_export() {
        assert!(read::compatible_json(&"[".repeat(1000)).is_err());
        assert!(read::compatible_json(r#"{"Version":3 "FileReferences":{}}"#).is_err());
        assert!(read::compatible_json(r#"{"Version":3} trailing garbage"#).is_err());
        assert_eq!(
            read::compatible_json("{key:'text', /* ] */ trailing:[1,],}").unwrap()["key"],
            "text"
        );
        let broken = b"{\"Version\":3,\"FileReferences\":{\"Moc\":\"cat.moc3\",\"Textures\":[\"texture.png\"]\n]\n}\n},\"Groups\":[{}]}";
        let mut normalized = false;
        unsafe {
            let doc = read::bongo_safe_model_json_parse(
                broken.as_ptr().cast(),
                broken.len(),
                &mut normalized,
            );
            assert!(!doc.is_null());
            assert!(normalized);
            assert_eq!(
                to_value((*doc).root, 0).unwrap()["Groups"]
                    .as_array()
                    .unwrap()
                    .len(),
                1
            );
            read::bongo_json_doc_free(doc);
        }
    }
    #[test]
    fn embedded_nul_and_large_integer_remain_exact() {
        let mut doc = Doc::new();
        doc.root = doc.from_value(
            serde_json::from_str(r#"{"name":"a\u0000b","id":18446744073709551615}"#).unwrap(),
        );
        let output = unsafe { to_value(doc.root, 0).unwrap() };
        assert_eq!(output["name"], "a\0b");
        assert_eq!(output["id"].as_u64(), Some(u64::MAX));
    }
    #[test]
    fn nodes_stay_stable_while_document_grows() {
        let mut doc = Doc::new();
        let pointer = doc.string("original");
        for _ in 0..10000 {
            doc.alloc(Data::Null);
        }
        assert!(matches!(unsafe { &(*pointer).data },Data::String(v,_) if v=="original"));
    }
}
