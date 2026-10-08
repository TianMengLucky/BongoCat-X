use serde_json::Value;
use std::collections::HashMap;

fn array<'a>(value: &'a Value, key: &str) -> Result<&'a Vec<Value>, String> {
    value[key]
        .as_array()
        .ok_or_else(|| format!("Missing INP array: {key}"))
}
fn floats(values: &[Value]) -> bool {
    values
        .iter()
        .all(|v| v.as_f64().is_some_and(|x| x.is_finite() && x.abs() < 1e8))
}
fn walk<'a>(
    node: &'a Value,
    nodes: &mut HashMap<u64, &'a Value>,
    textures: usize,
    composite: bool,
    depth: usize,
    vertices: &mut usize,
) -> Result<(), String> {
    if depth > 48 || nodes.len() >= 4096 {
        return Err("INP node tree exceeds limits".into());
    }
    let id = node["uuid"]
        .as_u64()
        .filter(|v| *v <= u32::MAX as u64)
        .ok_or("Invalid node UUID")?;
    if nodes.insert(id, node).is_some() {
        return Err("Duplicate INP node UUID".into());
    }
    let kind = node["type"].as_str().ok_or("Missing INP node type")?;
    if !matches!(kind, "Node" | "Part" | "Composite" | "SimplePhysics") {
        return Err(format!("Inox2D does not support node type {kind}"));
    }
    if composite && (kind == "Composite" || node["masks"].as_array().is_some_and(|v| !v.is_empty()))
    {
        return Err(
            "Inox2D does not support nested composites or masked composite children".into(),
        );
    }
    if kind == "Composite" && node["masks"].as_array().is_some_and(|v| !v.is_empty()) {
        return Err("Inox2D does not support masked composites".into());
    }
    if kind == "Part" {
        let mesh = &node["mesh"];
        let verts = array(mesh, "verts")?;
        let uvs = array(mesh, "uvs")?;
        let indices = array(mesh, "indices")?;
        *vertices += verts.len() / 2;
        if verts.is_empty()
            || verts.len() % 2 != 0
            || verts.len() != uvs.len()
            || !floats(verts)
            || !floats(uvs)
            || *vertices > 1_000_000
            || indices.len() > 3_000_000
            || indices.len() % 3 != 0
            || indices
                .iter()
                .any(|v| v.as_u64().is_none_or(|i| i >= (verts.len() / 2) as u64))
        {
            return Err("Invalid INP mesh vertices/UVs/indices".into());
        }
        let slots = array(node, "textures")?;
        if slots.is_empty()
            || slots.iter().enumerate().any(|(i, v)| {
                v.as_u64()
                    .is_none_or(|id| id >= textures as u64 && !(i > 0 && id == u32::MAX as u64))
            })
        {
            return Err("Invalid INP texture reference".into());
        }
    }
    if kind == "SimplePhysics"
        && !matches!(
            node["model_type"].as_str(),
            Some("Pendulum" | "SpringPendulum")
        )
    {
        return Err("Unsupported Inox2D physics model".into());
    }
    if let Some(children) = node["children"].as_array() {
        for child in children {
            walk(
                child,
                nodes,
                textures,
                composite || kind == "Composite",
                depth + 1,
                vertices,
            )?;
        }
    }
    Ok(())
}

pub fn validate(json: &Value, textures: usize) -> Result<(), String> {
    if json["nodes"]["type"] != "Node" {
        return Err("INP root must be a Node".into());
    }
    let mut nodes = HashMap::new();
    walk(&json["nodes"], &mut nodes, textures, false, 0, &mut 0)?;
    for node in nodes.values() {
        if let Some(masks) = node["masks"].as_array() {
            if masks.len() > 64 {
                return Err("Too many INP masks".into());
            }
            for mask in masks {
                let source = mask["source"]
                    .as_u64()
                    .and_then(|id| nodes.get(&id))
                    .ok_or("Missing mask source")?;
                if !matches!(source["type"].as_str(), Some("Part"))
                    || source["masks"].as_array().is_some_and(|m| !m.is_empty())
                {
                    return Err("Inox2D does not support recursively masked sources".into());
                }
            }
        }
    }
    let parameters = array(json, "param")?;
    if parameters.len() > 1024 {
        return Err("Too many INP parameters".into());
    }
    let mut names = std::collections::HashSet::new();
    for param in parameters {
        let name = param["name"].as_str().ok_or("Missing parameter name")?;
        if !names.insert(name) {
            return Err("Duplicate parameter name".into());
        }
        for key in ["min", "max", "defaults"] {
            let pair = array(param, key)?;
            if pair.len() != 2 || !floats(pair) {
                return Err("Invalid parameter range".into());
            }
        }
        for axis in 0..2 {
            let min = param["min"][axis].as_f64().unwrap();
            let max = param["max"][axis].as_f64().unwrap();
            if min > max
                || (axis == 0 && min == max)
                || (axis == 1 && param["is_vec2"] == true && min == max)
            {
                return Err("Empty or inverted INP parameter range".into());
            }
        }
        let axes = array(param, "axis_points")?;
        if axes.len() != 2 {
            return Err("INP parameters need two axes".into());
        }
        for axis in axes {
            let points = axis.as_array().ok_or("Invalid axis points")?;
            if points.is_empty()
                || points.len() > 256
                || !floats(points)
                || points.windows(2).any(|v| v[0].as_f64() >= v[1].as_f64())
            {
                return Err("Invalid or unsorted INP parameter axis".into());
            }
        }
        for binding in array(param, "bindings")? {
            let node = binding["node"]
                .as_u64()
                .and_then(|id| nodes.get(&id))
                .ok_or("Missing bound node")?;
            let property = binding["param_name"]
                .as_str()
                .ok_or("Missing binding property")?;
            if property == "opacity" {
                return Err("Pinned Inox2D does not apply opacity parameter bindings".into());
            }
            if property == "deform" && node["type"] != "Part" {
                return Err("Deform binding requires a Part".into());
            }
        }
    }
    if json["animations"].as_array().is_some_and(|v| !v.is_empty()) {
        return Err("Pinned Inox2D does not support Inochi2D animations".into());
    }
    Ok(())
}
