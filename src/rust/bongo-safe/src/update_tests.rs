use super::*;
use std::io::Write;

fn callbacks() -> Callbacks {
    Callbacks {
        cancelled: None,
        progress: None,
        userdata: std::ptr::null_mut(),
    }
}

#[test]
fn checksum_requires_the_matching_package_and_rejects_extra_records() {
    let name = "BongoCat-X-2.1.2-windows-x64-portable.zip";
    let hash = "0123456789abcdef".repeat(4);
    let valid = format!("{hash}  {name}\r\n");
    assert!(checksum(valid.as_bytes(), name).is_ok());
    assert!(checksum(format!("\u{feff}{hash} *{name}\n").as_bytes(), name).is_ok());
    assert!(checksum(valid.as_bytes(), "another.zip").is_err());
    assert!(checksum(format!("{valid}{valid}").as_bytes(), name).is_err());
    assert!(checksum(b"invalid app.zip", name).is_err());
}

#[test]
fn corrupt_package_is_rejected_before_any_application_file_changes() {
    let root = tempfile::tempdir().unwrap();
    let package = root.path().join("app.zip");
    fs::write(&package, b"downloaded bytes").unwrap();
    let digest: [u8; 32] = Sha256::digest(b"downloaded bytes").into();
    verify_file(&package, &digest, callbacks()).unwrap();
    fs::write(&package, b"damaged download").unwrap();
    assert!(verify_file(&package, &digest, callbacks()).is_err());
}

#[test]
fn archive_staging_preserves_existing_app_files_and_rejects_traversal() {
    let root = tempfile::tempdir().unwrap();
    let package = root.path().join("app.zip");
    let target = root.path().join("running");
    fs::create_dir(&target).unwrap();
    let executable = target.join("BongoCat.exe");
    fs::write(&executable, b"current").unwrap();
    let file = File::create(&package).unwrap();
    let mut zip = zip::ZipWriter::new(file);
    zip.start_file(
        "release/BongoCat.exe",
        zip::write::SimpleFileOptions::default(),
    )
    .unwrap();
    zip.write_all(b"next").unwrap();
    zip.finish().unwrap();
    let stage = root.path().join("stage");
    update_archive::extract(&package, &stage, true, callbacks()).unwrap();
    let (source, destination) = update_archive::layout(&stage, &executable).unwrap();
    assert_eq!(source, stage.join("release"));
    assert_eq!(destination, target);
    update_archive::validate_target(&source, &destination).unwrap();
    assert_eq!(fs::read(&executable).unwrap(), b"current");

    let mut zip = zip::ZipWriter::new(File::create(&package).unwrap());
    zip.start_file("../escaped.exe", zip::write::SimpleFileOptions::default())
        .unwrap();
    zip.write_all(b"outside").unwrap();
    zip.finish().unwrap();
    assert!(
        update_archive::extract(&package, &root.path().join("unsafe"), true, callbacks()).is_err()
    );
    assert!(!root.path().join("escaped.exe").exists());
}

#[test]
fn dropping_a_prepared_update_cleans_only_its_private_stage() {
    let root = tempfile::tempdir().unwrap();
    let executable = root.path().join("BongoCat.exe");
    fs::write(&executable, b"running").unwrap();
    let directory = tempfile::Builder::new()
        .prefix("bongo-update-")
        .tempdir_in(root.path())
        .unwrap();
    let stage = directory.path().to_owned();
    let update = BongoSafeUpdate {
        directory: Some(directory),
        stage: stage.clone(),
        package: stage.join("app.zip"),
        source: stage.join("payload"),
        target: root.path().to_owned(),
        executable: executable.clone(),
        installed: false,
        digest: [0; 32],
    };
    unsafe { bongo_safe_update_free(Box::into_raw(Box::new(update))) };
    assert!(!stage.exists());
    assert_eq!(fs::read(executable).unwrap(), b"running");
}
