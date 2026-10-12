//! Bounded extraction of verified release packages into a private staging tree.
use crate::http::{Callbacks, Failure, Outcome};
use std::collections::HashSet;
use std::fs::{self, File};
use std::io::{Read, Write};
use std::path::{Component, Path, PathBuf};

const MAX_FILES: usize = 10_000;
const MAX_EXPANDED: u64 = 2 * 1024 * 1024 * 1024;

pub(crate) fn io_error(error: std::io::Error) -> Failure {
    Failure(3, format!("Update file error: {error}"))
}

fn relative(name: &Path) -> Outcome<PathBuf> {
    let text = name
        .to_str()
        .ok_or_else(|| Failure(3, "Invalid archive path".into()))?;
    if text.is_empty()
        || text.bytes().any(|b| b < 0x20 || b == b'\\' || b == b':')
        || name
            .components()
            .any(|part| !matches!(part, Component::Normal(_)))
    {
        return Err(Failure(3, "Unsafe archive path".into()));
    }
    #[cfg(windows)]
    for part in name.components() {
        let name = part.as_os_str().to_string_lossy();
        let base = name.split('.').next().unwrap_or("").to_ascii_uppercase();
        if name.ends_with(['.', ' '])
            || matches!(base.as_str(), "CON" | "PRN" | "AUX" | "NUL")
            || (base.len() == 4
                && (base.starts_with("COM") || base.starts_with("LPT"))
                && base.as_bytes()[3].is_ascii_digit())
        {
            return Err(Failure(3, "Unsupported Windows archive path".into()));
        }
    }
    Ok(name.to_owned())
}

fn copy_entry(
    reader: &mut impl Read,
    target: &Path,
    expected: u64,
    callbacks: Callbacks,
) -> Outcome<()> {
    let mut output = File::create_new(target).map_err(io_error)?;
    let mut copied = 0u64;
    let mut chunk = [0u8; 64 * 1024];
    loop {
        callbacks.check()?;
        let count = reader.read(&mut chunk).map_err(io_error)?;
        if count == 0 {
            break;
        }
        copied += count as u64;
        if copied > expected {
            return Err(Failure(3, "Archive size mismatch".into()));
        }
        output.write_all(&chunk[..count]).map_err(io_error)?;
    }
    if copied != expected {
        return Err(Failure(3, "Incomplete archive entry".into()));
    }
    Ok(())
}

fn output_path(
    root: &Path,
    name: &Path,
    size: u64,
    total: &mut u64,
    seen: &mut HashSet<String>,
) -> Outcome<PathBuf> {
    let name = relative(name)?;
    let mut key = name.to_string_lossy().into_owned();
    if cfg!(windows) {
        key = key.to_lowercase();
    }
    if seen.len() >= MAX_FILES || !seen.insert(key) || size > MAX_EXPANDED - *total {
        return Err(Failure(3, "Duplicate or oversized release archive".into()));
    }
    *total += size;
    let target = root.join(name);
    if let Some(parent) = target.parent() {
        fs::create_dir_all(parent).map_err(io_error)?;
    }
    Ok(target)
}

#[cfg(unix)]
fn permissions(path: &Path, mode: u32) -> Outcome<()> {
    use std::os::unix::fs::PermissionsExt;
    fs::set_permissions(path, fs::Permissions::from_mode(mode & 0o777)).map_err(io_error)
}
#[cfg(not(unix))]
fn permissions(_path: &Path, _mode: u32) -> Outcome<()> {
    Ok(())
}

pub(crate) fn extract(
    package: &Path,
    root: &Path,
    zip_format: bool,
    callbacks: Callbacks,
) -> Outcome<()> {
    fs::create_dir(root).map_err(io_error)?;
    let mut total = 0;
    let mut seen = HashSet::new();
    let file = File::open(package).map_err(io_error)?;
    if zip_format {
        let mut archive =
            zip::ZipArchive::new(file).map_err(|_| Failure(3, "Invalid release ZIP".into()))?;
        if archive.len() > MAX_FILES {
            return Err(Failure(3, "Too many archive entries".into()));
        }
        for index in 0..archive.len() {
            callbacks.check()?;
            let mut entry = archive
                .by_index(index)
                .map_err(|_| Failure(3, "Invalid ZIP entry".into()))?;
            let mode = entry.unix_mode().unwrap_or(0o644);
            if mode & 0o170000 != 0 && !matches!(mode & 0o170000, 0o100000 | 0o040000) {
                return Err(Failure(
                    3,
                    "Links are not allowed in release archives".into(),
                ));
            }
            let name = PathBuf::from(entry.name());
            let size = entry.size();
            let target = output_path(root, &name, size, &mut total, &mut seen)?;
            if entry.is_dir() {
                fs::create_dir_all(target).map_err(io_error)?;
            } else {
                copy_entry(&mut entry, &target, size, callbacks)?;
                permissions(&target, mode)?;
            }
        }
    } else {
        let mut archive = tar::Archive::new(flate2::read::GzDecoder::new(file));
        for entry in archive.entries().map_err(io_error)? {
            callbacks.check()?;
            let mut entry = entry.map_err(io_error)?;
            let kind = entry.header().entry_type();
            if !kind.is_file() && !kind.is_dir() {
                return Err(Failure(
                    3,
                    "Links are not allowed in release archives".into(),
                ));
            }
            let size = entry.size();
            let mode = entry.header().mode().map_err(io_error)?;
            let target = output_path(
                root,
                &entry.path().map_err(io_error)?,
                size,
                &mut total,
                &mut seen,
            )?;
            if kind.is_dir() {
                fs::create_dir_all(target).map_err(io_error)?;
            } else {
                copy_entry(&mut entry, &target, size, callbacks)?;
                permissions(&target, mode)?;
            }
        }
    }
    Ok(())
}

pub(crate) fn files(root: &Path) -> Outcome<Vec<PathBuf>> {
    let mut pending = vec![root.to_owned()];
    let mut result = Vec::new();
    let mut entries = 0;
    while let Some(directory) = pending.pop() {
        for entry in fs::read_dir(directory).map_err(io_error)? {
            let entry = entry.map_err(io_error)?;
            entries += 1;
            if entries > MAX_FILES {
                return Err(Failure(3, "Too many update files".into()));
            }
            let kind = entry.file_type().map_err(io_error)?;
            if kind.is_symlink() {
                return Err(Failure(3, "Unexpected staged link".into()));
            }
            if kind.is_dir() {
                pending.push(entry.path());
            } else if kind.is_file() {
                result.push(entry.path());
            } else {
                return Err(Failure(3, "Unexpected staged file type".into()));
            }
        }
    }
    result.sort();
    Ok(result)
}

pub(crate) fn layout(root: &Path, executable: &Path) -> Outcome<(PathBuf, PathBuf)> {
    let expected = executable
        .file_name()
        .ok_or_else(|| Failure(3, "Missing executable name".into()))?;
    let candidates: Vec<_> = files(root)?
        .into_iter()
        .filter(|path| path.file_name() == Some(expected))
        .collect();
    if candidates.len() != 1 {
        return Err(Failure(
            3,
            "Release must contain one matching executable".into(),
        ));
    }
    let payload = &candidates[0];
    #[cfg(target_os = "macos")]
    {
        let source = payload
            .ancestors()
            .find(|path| path.extension().is_some_and(|e| e == "app"));
        let target = executable
            .ancestors()
            .find(|path| path.extension().is_some_and(|e| e == "app"));
        if let (Some(source), Some(target)) = (source, target) {
            return Ok((source.to_owned(), target.to_owned()));
        }
    }
    Ok((
        payload.parent().unwrap().to_owned(),
        executable.parent().unwrap().to_owned(),
    ))
}

pub(crate) fn validate_target(source: &Path, target: &Path) -> Outcome<()> {
    for file in files(source)? {
        let relative = file
            .strip_prefix(source)
            .map_err(|_| Failure(3, "Invalid staged file".into()))?;
        let destination = target.join(relative);
        for part in destination.ancestors() {
            match fs::symlink_metadata(part) {
                Ok(meta) if meta.file_type().is_symlink() => {
                    return Err(Failure(3, "Update target contains a link".into()))
                }
                Ok(_) => {}
                Err(error) if error.kind() == std::io::ErrorKind::NotFound => {}
                Err(error) => return Err(io_error(error)),
            }
        }
    }
    // Detect read-only application directories before closing the running app.
    tempfile::NamedTempFile::new_in(target).map_err(io_error)?;
    Ok(())
}
