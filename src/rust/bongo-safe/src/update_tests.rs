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

#[cfg(windows)]
fn run_helper(rollback: bool, target_root: Option<&Path>) {
    use std::process::Command;
    let root = tempfile::tempdir().unwrap();
    let stage = root.path().join("stage");
    let source = stage.join("payload");
    let target_directory = target_root.map(|path| tempfile::tempdir_in(path).unwrap());
    let target = target_directory
        .as_ref()
        .map_or_else(|| root.path().join("app"), |dir| dir.path().join("app"));
    fs::create_dir_all(&source).unwrap();
    fs::create_dir_all(&target).unwrap();
    let executable = target.join("BongoCat.exe");
    fs::write(&executable, b"old application").unwrap();
    fs::write(source.join("BongoCat.exe"), b"new application").unwrap();
    fs::write(source.join("new.txt"), b"new asset").unwrap();
    fs::write(target.join("settings.json"), b"user settings").unwrap();
    let update = BongoSafeUpdate {
        directory: None,
        stage: stage.clone(),
        package: stage.join("release.zip"),
        source,
        target: target.clone(),
        executable: executable.clone(),
        installed: false,
        digest: [0; 32],
    };
    // Exercise the real helper without waiting for this test process or
    // launching an application. Record the relaunch request instead.
    let mut body = crate::update_apply::script(&update, "").unwrap();
    body = body.replace(
        &format!(
            "[Diagnostics.Process]::GetProcessById({})",
            std::process::id()
        ),
        "[Diagnostics.Process]::GetProcessById(-1)",
    );
    body = body.replace(
        "function Relaunch { Start-Process -FilePath $exe -ArgumentList ('\"--nearby-root=' + $nearby + '\"') -WorkingDirectory $target }",
        "function Relaunch { Set-Content -LiteralPath (Join-Path $stage 'relaunched') -Value 'yes' }",
    );
    assert!(!body.contains("Start-Process"));
    if rollback {
        body = body.replace(
            "    Relaunch\n",
            "    throw 'Injected failure after replacement'\n",
        );
    }
    let helper = stage.join("test.ps1");
    fs::write(&helper, format!("\u{feff}{body}")).unwrap();
    let system = std::env::var_os("SystemRoot").unwrap();
    let result =
        Command::new(Path::new(&system).join("System32/WindowsPowerShell/v1.0/powershell.exe"))
            .args([
                "-NoProfile",
                "-NonInteractive",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
            ])
            .arg(helper)
            .output()
            .unwrap();
    assert_eq!(result.status.success(), !rollback);
    assert!(stage.join("relaunched").exists());
    assert_eq!(
        fs::read(target.join("settings.json")).unwrap(),
        b"user settings"
    );
    assert!(!fs::read_dir(&target).unwrap().any(|entry| entry
        .unwrap()
        .file_name()
        .to_string_lossy()
        .contains(".bongo-update-")));
    if rollback {
        assert_eq!(fs::read(executable).unwrap(), b"old application");
        assert!(!target.join("new.txt").exists());
        assert!(stage.join("update-error.log").exists());
    } else {
        assert_eq!(fs::read(executable).unwrap(), b"new application");
        assert_eq!(fs::read(target.join("new.txt")).unwrap(), b"new asset");
    }
}

#[cfg(windows)]
#[test]
fn portable_helper_replaces_existing_files_and_preserves_user_data() {
    run_helper(false, None);
}

#[cfg(windows)]
#[test]
fn portable_helper_rolls_back_replaced_and_new_files() {
    run_helper(true, None);
}

#[cfg(windows)]
#[test]
fn portable_helper_updates_and_rolls_back_across_volumes() {
    let cache = fs::canonicalize(std::env::temp_dir()).unwrap();
    let volume = cache.components().next().unwrap();
    for drive in b'A'..=b'Z' {
        let root = PathBuf::from(format!("{}:\\", drive as char));
        if let Ok(directory) = tempfile::tempdir_in(&root) {
            let target = fs::canonicalize(directory.path()).unwrap();
            if target.components().next() != Some(volume) {
                run_helper(false, Some(directory.path()));
                run_helper(true, Some(directory.path()));
                return;
            }
        }
    }
    eprintln!("Skipping cross-volume update test: no second writable volume");
}
