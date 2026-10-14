//! Shared catalog parser, compiled into the optional Rust store plugin.
use reqwest::Url;
use serde::Deserialize;

#[derive(Clone, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Model {
    #[serde(alias = "model_name")]
    pub name: String,
    pub author: String,
    #[serde(default, alias = "authorUrl")]
    pub author_url: String,
    #[serde(alias = "repositoryPath", alias = "file_path")]
    pub repository_path: String,
    #[serde(alias = "folderPath", alias = "model_folder")]
    pub folder_path: String,
    #[serde(alias = "previewPath")]
    pub preview_path: String,
    pub sha256: String,
}
#[derive(Deserialize)]
#[serde(untagged)]
enum Catalog {
    List(Vec<Model>),
    Wrapped { models: Vec<Model> },
}
fn text(value: &str, capacity: usize) -> bool {
    !value.trim().is_empty() && value.len() < capacity && !value.chars().any(char::is_control)
}
fn path(value: &str) -> bool {
    text(value, 1024)
        && value
            .split('/')
            .all(|part| !part.is_empty() && part != "." && part != "..")
        && !value.contains(['\\', ':', '?', '#', '%'])
}
pub fn link(path: &str, kind: &str) -> String {
    let mut url = Url::parse("https://github.com/TianMengLucky/Awesome-BongoCat/").unwrap();
    {
        let mut segments = url.path_segments_mut().unwrap();
        segments.pop_if_empty().push(kind).push("main");
        for part in path.split('/') {
            segments.push(part);
        }
    }
    url.into()
}
pub fn raw_link(path: &str) -> String {
    let mut url =
        Url::parse("https://raw.githubusercontent.com/TianMengLucky/Awesome-BongoCat/main/")
            .unwrap();
    {
        let mut segments = url.path_segments_mut().unwrap();
        segments.pop_if_empty();
        for part in path.split('/') {
            segments.push(part);
        }
    }
    url.into()
}
pub fn parse(bytes: &[u8]) -> Result<Vec<Model>, ()> {
    if bytes.len() > 2 * 1024 * 1024 {
        return Err(());
    }
    let catalog: Catalog = serde_json::from_slice(bytes).map_err(|_| ())?;
    let models = match catalog {
        Catalog::List(v) | Catalog::Wrapped { models: v } => v,
    };
    if models.len() > 512 {
        return Err(());
    }
    for model in &models {
        let author_valid = model.author_url.is_empty()
            || Url::parse(&model.author_url).is_ok_and(|url| {
                text(&model.author_url, 1024)
                    && url.scheme() == "https"
                    && url.host_str().is_some()
                    && url.username().is_empty()
                    && url.password().is_none()
            });
        if !text(&model.name, 256)
            || !text(&model.author, 256)
            || !author_valid
            || !path(&model.repository_path)
            || !path(&model.folder_path)
            || !path(&model.preview_path)
            || !model.repository_path.to_ascii_lowercase().ends_with(".zip")
            || model.sha256.len() != 64
            || !model.sha256.bytes().all(|b| b.is_ascii_hexdigit())
            || link(&model.repository_path, "blob").len() >= 2048
            || link(&model.folder_path, "tree").len() >= 2048
            || raw_link(&model.preview_path).len() >= 2048
            || raw_link(&model.repository_path).len() >= 2048
        {
            return Err(());
        }
    }
    Ok(models)
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn empty_catalogs() {
        assert!(parse(b"[]").unwrap().is_empty());
        assert!(parse(br#"{"models":[]}"#).unwrap().is_empty());
        assert!(parse(b"{}").is_err());
    }
    #[test]
    fn reject_incomplete_download_metadata() {
        let value = serde_json::json!({"name":"Cat", "author":"A", "repository_path":"m/c.zip",
            "folder_path":"m", "preview_path":"m/p.png", "sha256":"a".repeat(64)});
        assert!(parse(
            serde_json::to_string(&vec![value.clone()])
                .unwrap()
                .as_bytes()
        )
        .is_ok());
        for field in ["sha256", "preview_path", "repository_path"] {
            let mut bad = value.clone();
            bad.as_object_mut().unwrap().remove(field);
            assert!(parse(serde_json::to_string(&vec![bad]).unwrap().as_bytes()).is_err());
        }
        let mut bad = value.clone();
        bad["sha256"] = serde_json::json!("g".repeat(64));
        assert!(parse(serde_json::to_string(&vec![bad]).unwrap().as_bytes()).is_err());
        assert!(parse(serde_json::to_string(&vec![value; 513]).unwrap().as_bytes()).is_err());
    }
    #[test]
    fn validate_catalog() {
        let valid = br#"[{"name":"Cat","author":"A","author_url":"https://example.com","repository_path":"models/cat/cat.zip","folder_path":"models/cat","preview_path":"models/cat/preview.png","sha256":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}]"#;
        assert_eq!(parse(valid).unwrap()[0].name, "Cat");
        for bad in ["../cat", "/cat", "cat/../x", "cat%2f.."] {
            assert!(!path(bad));
        }
        assert!(link("models/my cat", "tree").ends_with("models/my%20cat"));
        let bad = String::from_utf8(valid.to_vec())
            .unwrap()
            .replace("https://example.com", "javascript:alert(1)");
        assert!(parse(bad.as_bytes()).is_err());
    }
}
