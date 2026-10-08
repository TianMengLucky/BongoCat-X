use super::payload;

#[test]
fn initialize_valid_textured_model() {
    let node = serde_json::json!({"uuid":0,"type":"Node","name":"root",
        "enabled":true,"zsort":0,"lockToRoot":false,
        "transform":{"trans":[0,0,0],"rot":[0,0,0],"scale":[1,1]}});
    let mut part = node.clone();
    part["uuid"] = 1.into();
    part["type"] = "Part".into();
    part["textures"] = serde_json::json!([0]);
    part["blend_mode"] = "Normal".into();
    part["mesh"] = serde_json::json!({"verts":[0,0,100,0,0,100],
        "uvs":[0,0,1,0,0,1],"indices":[0,1,2],"origin":[0,0]});
    let mut root = node;
    root["children"] = serde_json::json!([part]);
    let json = serde_json::json!({"nodes":root,"param":[],
        "physics":{"pixelsPerMeter":100,"gravity":9.8},
        "meta":{"name":"fixture","version":"1.0","rigger":null,"artist":null,
            "copyright":null,"licenseURL":null,"contact":null,"reference":null,
            "preservePixels":false}});
    let mut png = std::io::Cursor::new(Vec::new());
    image::RgbaImage::from_pixel(2, 2, image::Rgba([255, 0, 0, 255]))
        .write_to(&mut png, image::ImageFormat::Png)
        .unwrap();
    let png = png.into_inner();
    let json = serde_json::to_vec(&json).unwrap();
    let mut bytes = b"TRNSRTS\0".to_vec();
    bytes.extend_from_slice(&(json.len() as u32).to_be_bytes());
    bytes.extend_from_slice(&json);
    bytes.extend_from_slice(b"TEX_SECT");
    bytes.extend_from_slice(&1u32.to_be_bytes());
    bytes.extend_from_slice(&(png.len() as u32).to_be_bytes());
    bytes.push(0);
    bytes.extend_from_slice(&png);
    let (model, _) = super::parse(&bytes).unwrap();
    let buffers = &model.puppet.render_ctx.unwrap().vertex_buffers;
    assert_eq!(buffers.indices.len(), 9); // Inox2D also reserves the composite quad.
    assert_eq!(model.textures.len(), 1);
}

#[test]
fn reject_live2d_and_truncated_container() {
    assert!(payload(br#"{"Version":3,"FileReferences":{"Moc":"cat.moc3"}}"#).is_err());
    assert!(payload(b"TRNSRTS\0\0\0\0\x20{}").is_err());
}

#[test]
fn reject_hostile_section_lengths_before_allocation() {
    let mut bytes = b"TRNSRTS\0".to_vec();
    bytes.extend_from_slice(&u32::MAX.to_be_bytes());
    assert!(payload(&bytes).unwrap_err().contains("size limit"));
}

#[test]
fn reject_unsupported_nodes_instead_of_silently_omitting_them() {
    let json = serde_json::json!({"param":[], "nodes":{"uuid":0,"type":"Node","children":[{"uuid":1,"type":"MeshGroup"}]}});
    assert!(super::schema::validate(&json, 0)
        .unwrap_err()
        .contains("MeshGroup"));
}

#[test]
fn reject_invalid_indices_and_recursive_masks() {
    let part = serde_json::json!({"uuid":1,"type":"Part","textures":[0],
        "mesh":{"verts":[0,0,1,0,0,1],"uvs":[0,0,1,0,0,1],"indices":[0,1,8]}});
    let mut json =
        serde_json::json!({"param":[], "nodes":{"uuid":0,"type":"Node","children":[part]}});
    assert!(super::schema::validate(&json, 1).is_err());
    json["nodes"]["children"][0]["mesh"]["indices"] = serde_json::json!([0, 1, 2]);
    json["nodes"]["children"][0]["masks"] = serde_json::json!([{"source":1,"mode":"Mask"}]);
    assert!(super::schema::validate(&json, 1)
        .unwrap_err()
        .contains("recursively"));
}
