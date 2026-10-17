use super::*;
#[test]
fn adapter_conversion_preserves_comment_labels_audio_and_render_calibration() {
    let root = tempfile::tempdir().unwrap();
    let config = root.path().join("config.json");
    let assets = root.path().join("assets");
    let target = root.path().join("target");
    fs::create_dir(&target).unwrap();
    fs::create_dir_all(assets.join("sounds")).unwrap();
    fs::write(assets.join("sounds/0.wav"), b"authored sound bytes").unwrap();
    fs::write(root.path().join("model.model3.json"), b"{}").unwrap();
    fs::write(
        &config,
        "{standard:{sounds:[[65, // Drum\n66]]},decoration:{l2d_correct:2,window_size:[800,600]}}",
    )
    .unwrap();
    let package = Package {
        config: &config,
        directory: root.path(),
        setting: Path::new("model.model3.json"),
        assets: &assets,
        overrides: None,
        target: &target,
        mode: "standard",
        format: 1,
        gamepad_buttons: false,
    };
    let adapter = convert(&package).unwrap();
    assert_eq!(adapter["render"]["projectionScale"].as_f64(), Some(2.0));
    assert_eq!(adapter["render"]["referenceWidth"], 800);
    assert_eq!(adapter["bindings"][0]["kind"], "sound");
    assert_eq!(adapter["bindings"][0]["label"], "Drum");
    assert!(adapter["bindings"][0].get("shortcut").is_none());
    assert_eq!(
        fs::read(target.join("resources/sounds/0.wav")).unwrap(),
        b"authored sound bytes"
    );
}
