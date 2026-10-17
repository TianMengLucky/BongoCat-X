use super::*;
fn empty(row: &Value) -> bool {
    row.is_null() || row.as_array().is_some_and(Vec::is_empty)
}
fn manifest(package: &Package<'_>) -> Outcome<Value> {
    let setting = crate::archive::relative(
        package
            .setting
            .to_str()
            .ok_or_else(|| failure("Invalid manifest path"))?,
    )?;
    let text = read(&package.directory.join(setting))?;
    if text.len() > 4 * 1024 * 1024 {
        return Err(failure("Live2D manifest exceeds 4 MiB"));
    }
    unsafe {
        let doc = crate::json_dom::read::bongo_safe_model_json_parse(
            text.as_ptr().cast(),
            text.len(),
            std::ptr::null_mut(),
        );
        if doc.is_null() {
            return Err(failure("Invalid Live2D manifest"));
        }
        let doc = Box::from_raw(doc);
        crate::json_dom::to_value(doc.root, 0).map_err(|e| failure(&e))
    }
}
fn behaviors(
    package: &Package<'_>,
    config: &Value,
    labels: &std::collections::HashMap<(String, usize), String>,
    items: &mut Vec<Value>,
) -> Outcome<()> {
    let manifest = manifest(package)?;
    for (field, kind, group) in [
        ("l2d_expression", "expression", None),
        ("l2d_motion", "motion", Some("CAT_motion")),
        ("l2d_motion_lockhand", "motion", Some("CAT_motion_lock")),
    ] {
        let mode = if field == "l2d_motion_lockhand" || package.mode == "gamepad" {
            "standard"
        } else {
            package.mode
        };
        let rows = &config[mode][field];
        if rows.is_null() {
            continue;
        }
        let available = if let Some(group) = group {
            &manifest["FileReferences"]["Motions"][group]
        } else {
            &manifest["FileReferences"]["Expressions"]
        };
        if group.is_some() && rows.is_array() && !available.is_array() {
            continue;
        }
        let rows = rows
            .as_array()
            .ok_or_else(|| failure("Mver behavior bindings must be arrays"))?;
        for (index, row) in rows
            .iter()
            .enumerate()
            .take(available.as_array().map_or(0, Vec::len))
        {
            if empty(row) {
                continue;
            }
            let Some(file) = available[index]["File"].as_str() else {
                continue;
            };
            if !crate::archive::relative(file)
                .ok()
                .is_some_and(|p| package.directory.join(p).is_file())
            {
                continue;
            }
            if !chord::valid(row, false, false) {
                return Err(failure("Unsupported Mver behavior chord"));
            }
            let mut item = json!({"kind":kind,"index":index});
            if let Some(group) = group {
                item["group"] = json!(group);
            }
            if let Some(label) = labels.get(&(field.to_owned(), index)) {
                item["label"] = json!(label);
            }
            items.push(item);
        }
    }
    Ok(())
}
fn sounds(
    package: &Package<'_>,
    config: &Value,
    labels: &std::collections::HashMap<(String, usize), String>,
    items: &mut Vec<Value>,
) -> Outcome<()> {
    let rows = &config[package.mode]["sounds"];
    if rows.is_null() {
        return Ok(());
    }
    let rows = rows
        .as_array()
        .ok_or_else(|| failure("Mver sounds must be an array"))?;
    let target = package.target.join("resources/sounds");
    fs::create_dir_all(&target).map_err(io_error)?;
    let mut emitted = false;
    for (index, row) in rows.iter().enumerate() {
        if empty(row) {
            continue;
        }
        let source = ["wav", "ogg", "flac"]
            .iter()
            .map(|ext| package.assets.join(format!("sounds/{index}.{ext}")))
            .find(|p| p.is_file());
        let Some(source) = source else {
            continue;
        };
        if !chord::valid(row, false, true) {
            return Err(failure("Unsupported Mver audio chord"));
        }
        let name = source
            .file_name()
            .ok_or_else(|| failure("Invalid sound filename"))?;
        fs::copy(&source, target.join(name)).map_err(io_error)?;
        let mut item = json!({"kind":"sound","sound":format!("resources/sounds/{}",name.to_string_lossy()),
            "overlap":config["decoration"]["soundKeep"].as_bool().unwrap_or(config["decoration"].get("soundKeep").is_none())});
        if let Some(label) = labels.get(&("sounds".into(), index)) {
            item["label"] = json!(label);
        }
        items.push(item);
        emitted = true;
    }
    let clear = &config["decoration"]["soundClear"];
    if emitted && !clear.is_null() && !clear.as_array().is_some_and(Vec::is_empty) {
        items.push(json!({"kind":"sound-clear"}));
    }
    Ok(())
}
fn effects(package: &Package<'_>, config: &Value, items: &mut Vec<Value>) -> Outcome<()> {
    let Some(rows) = config[package.mode]["face"]
        .as_array()
        .filter(|r| !r.is_empty())
    else {
        return Ok(());
    };
    let momentary = !config["decoration"]["emoticonKeep"]
        .as_bool()
        .unwrap_or(false);
    let target = package.target.join("resources/effects");
    fs::create_dir_all(&target).map_err(io_error)?;
    let mut emitted = false;
    for (index, row) in rows.iter().enumerate() {
        if empty(row) {
            continue;
        }
        let name = format!("{index}.png");
        let Some(source) =
            asset(package, &Path::new("face").join(&name)).filter(|p| dimensions(p).is_some())
        else {
            continue;
        };
        if !chord::valid(row, package.gamepad_buttons, false) {
            return Err(failure("Unsupported Mver effect chord"));
        }
        fs::copy(source, target.join(&name)).map_err(io_error)?;
        let mut item =
            json!({"kind":"effect","index":index,"effect":format!("resources/effects/{name}")});
        if momentary {
            item["momentary"] = json!(true);
        }
        items.push(item);
        emitted = true;
    }
    let clear = &config["decoration"]["emoticonClear"];
    if emitted && !momentary && !empty(clear) {
        if !chord::valid(clear, package.gamepad_buttons, false) {
            return Err(failure("Unsupported Mver effect-clear chord"));
        }
        items.push(json!({"kind":"effect-clear"}));
    }
    Ok(())
}
pub(super) fn convert(package: &Package<'_>, output: &mut Value) -> Outcome<()> {
    let text = read(package.config)?;
    let config = parse(&text)?;
    for mode in [package.mode, "standard"] {
        for field in [
            "l2d_expression",
            "l2d_motion",
            "l2d_motion_lockhand",
            "sounds",
            "face",
        ] {
            if config[mode][field]
                .as_array()
                .is_some_and(|rows| rows.len() > 5120)
            {
                return Err(failure("Too many Mver bindings"));
            }
        }
    }
    if !config[package.mode].is_object() {
        return Err(failure("Missing Mver mode"));
    }
    let labels = crate::mver_json::labels(&text, package.mode).unwrap_or_default();
    render::metadata(package, &config, output);
    let mut items = Vec::new();
    behaviors(package, &config, &labels, &mut items)?;
    sounds(package, &config, &labels, &mut items)?;
    effects(package, &config, &mut items)?;
    output["bindings"] = Value::Array(items);
    Ok(())
}
