//! Deferred replacement runs only after the user confirms and the host exits.
use crate::http::{Failure, Outcome};
use crate::update::BongoSafeUpdate;
#[cfg(unix)]
use crate::update_archive::files;
use crate::update_archive::io_error;
use std::fs;
use std::path::Path;
use std::process::Command;
use std::time::{Duration, Instant};

fn quote(text: &str) -> String {
    if cfg!(windows) {
        format!("'{}'", text.replace('\'', "''"))
    } else {
        format!("'{}'", text.replace('\'', "'\"'\"'"))
    }
}

fn path(path: &Path) -> Outcome<String> {
    let text = path
        .to_str()
        .ok_or_else(|| Failure(3, "Invalid update path".into()))?;
    #[cfg(windows)]
    {
        if let Some(unc) = text.strip_prefix(r"\\?\UNC\") {
            return Ok(quote(&format!(r"\\{unc}")));
        }
        if let Some(local) = text.strip_prefix(r"\\?\") {
            return Ok(quote(local));
        }
    }
    Ok(quote(text))
}

#[cfg(windows)]
pub(crate) fn script(update: &BongoSafeUpdate, nearby: &str) -> Outcome<String> {
    let mut body = format!(
        r#"$ErrorActionPreference = 'Stop'
$stage = {stage}
$source = {source}
$target = {target}
$exe = {exe}
$package = {package}
$nearby = {nearby}
$journal = [Collections.Generic.List[object]]::new()
function Relaunch {{ Start-Process -FilePath $exe -ArgumentList ('"--nearby-root=' + $nearby + '"') -WorkingDirectory $target }}
try {{
    Set-Content -LiteralPath (Join-Path $stage 'ready') -Value 'ready' -Encoding ascii
    try {{ $parent = [Diagnostics.Process]::GetProcessById({pid}) }} catch {{ $parent = $null }}
    if ($parent -and !$parent.WaitForExit(60000)) {{ throw 'Application did not exit in time' }}
"#,
        stage = path(&update.stage)?,
        source = path(&update.source)?,
        target = path(&update.target)?,
        exe = path(&update.executable)?,
        package = path(&update.package)?,
        nearby = quote(nearby),
        pid = std::process::id()
    );
    if update.installed {
        body.push_str(r#"    $installer = Start-Process -FilePath $package -ArgumentList '/SILENT','/SUPPRESSMSGBOXES','/NORESTART','/CLOSEAPPLICATIONS' -Wait -PassThru
    if ($installer.ExitCode -ne 0) { throw ('Installer failed: ' + $installer.ExitCode) }
"#);
    } else {
        body.push_str(r#"    foreach ($file in Get-ChildItem -LiteralPath $source -File -Recurse) {
        $relative = $file.FullName.Substring($source.Length + 1)
        $destination = Join-Path $target $relative
        New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($destination)) -Force | Out-Null
        $existed = Test-Path -LiteralPath $destination -PathType Leaf
        $temporary = $destination + '.bongo-update-' + $PID
        # File.Replace requires its backup on the destination volume, which
        # may differ from the cache volume. Keep it beside the destination.
        $saved = $temporary + '.backup'
        if ((Test-Path -LiteralPath $temporary) -or (Test-Path -LiteralPath $saved)) {
            throw 'Update temporary file already exists'
        }
        if ($existed) { Copy-Item -LiteralPath $destination -Destination $saved }
        $journal.Add(@{ Destination=$destination; Saved=$saved; Existed=$existed; Temporary=$temporary })
        Copy-Item -LiteralPath $file.FullName -Destination $temporary
        # PowerShell coerces $null to an empty string for this .NET overload.
        # Use a real backup path; File.Replace atomically saves the old file.
        if ($existed) { [IO.File]::Replace($temporary, $destination, $saved) }
        else { [IO.File]::Move($temporary, $destination) }
    }
"#);
    }
    body.push_str(r#"    Relaunch
    foreach ($entry in $journal) {
        Remove-Item -LiteralPath $entry.Saved -Force -ErrorAction SilentlyContinue
    }
    $resolved = [IO.Path]::GetFullPath($stage)
    if ($resolved -eq $stage -and [IO.Path]::GetFileName($resolved).StartsWith('bongo-update-')) {
        Remove-Item -LiteralPath $resolved -Recurse -Force -ErrorAction SilentlyContinue
    }
} catch {
    $_ | Out-File -LiteralPath (Join-Path $stage 'update-error.log')
    for ($i = $journal.Count - 1; $i -ge 0; $i--) {
        $entry = $journal[$i]
        try {
            if ($entry.Existed) { Copy-Item -LiteralPath $entry.Saved -Destination $entry.Destination -Force }
            else { Remove-Item -LiteralPath $entry.Destination -Force -ErrorAction SilentlyContinue }
            Remove-Item -LiteralPath $entry.Temporary -Force -ErrorAction SilentlyContinue
            Remove-Item -LiteralPath $entry.Saved -Force -ErrorAction SilentlyContinue
        } catch { $_ | Out-File -LiteralPath (Join-Path $stage 'update-error.log') -Append }
    }
    if (!$parent -or $parent.HasExited) { Relaunch }
    exit 1
}
"#);
    Ok(body)
}

#[cfg(unix)]
fn script(update: &BongoSafeUpdate, nearby: &str) -> Outcome<String> {
    let mut body = format!(
        r#"#!/bin/sh
set -eu
stage={stage}
source={source}
target={target}
exe={exe}
nearby={nearby}
backup="$stage/backup"
journal="$stage/journal"
mkdir -p "$backup"
: > "$journal"
exec >> "$stage/update-error.log" 2>&1
relaunch() {{ (cd "$target" && "$exe" "--nearby-root=$nearby" >/dev/null 2>&1) & }}
rollback() {{
    trap - EXIT HUP INT TERM
    while IFS= read -r rel; do
        destination="$target/$rel"
        saved="$backup/files/$rel"
        missing="$backup/missing/$rel"
        if [ -f "$saved" ]; then cp -p "$saved" "$destination" || :
        elif [ -f "$missing" ]; then rm -f "$destination" || :; fi
        rm -f "$destination.bongo-update-{pid}" || :
    done < "$journal"
    relaunch
    exit 1
}}
: > "$stage/ready"
attempt=0
while kill -0 {pid} 2>/dev/null; do
    attempt=$((attempt + 1))
    [ "$attempt" -le 60 ] || exit 1
    sleep 1
done
trap rollback EXIT HUP INT TERM
apply_one() {{
    rel="$1"
    destination="$target/$rel"
    saved="$backup/files/$rel"
    missing="$backup/missing/$rel"
    mkdir -p "$(dirname "$destination")" "$(dirname "$saved")" "$(dirname "$missing")"
    if [ -f "$destination" ]; then cp -p "$destination" "$saved"
    else : > "$missing"; fi
    printf '%s\n' "$rel" >> "$journal"
    cp -p "$source/$rel" "$destination.bongo-update-{pid}"
    mv -f "$destination.bongo-update-{pid}" "$destination"
}}
"#,
        stage = path(&update.stage)?,
        source = path(&update.source)?,
        target = path(&update.target)?,
        exe = path(&update.executable)?,
        nearby = quote(nearby),
        pid = std::process::id()
    );
    for file in files(&update.source)? {
        let rel = file
            .strip_prefix(&update.source)
            .map_err(|_| Failure(3, "Invalid staged path".into()))?;
        body.push_str(&format!("apply_one {}\n", path(rel)?));
    }
    body.push_str("trap - EXIT HUP INT TERM\nrelaunch\n");
    body.push_str(&format!("rm -rf -- {}\n", path(&update.stage)?));
    Ok(body)
}

pub(crate) fn launch(update: &BongoSafeUpdate, nearby: &str) -> Outcome<()> {
    let body = script(update, nearby)?;
    #[cfg(windows)]
    let helper = update.stage.join("apply.ps1");
    #[cfg(unix)]
    let helper = update.stage.join("apply.sh");
    #[cfg(windows)]
    fs::write(&helper, format!("\u{feff}{body}")).map_err(io_error)?;
    #[cfg(unix)]
    fs::write(&helper, body).map_err(io_error)?;
    #[cfg(windows)]
    let mut command = {
        use std::os::windows::process::CommandExt;
        let system = std::env::var_os("SystemRoot")
            .ok_or_else(|| Failure(3, "Windows system directory is unavailable".into()))?;
        let powershell = Path::new(&system).join("System32/WindowsPowerShell/v1.0/powershell.exe");
        let mut command = Command::new(powershell);
        command
            .args([
                "-NoProfile",
                "-NonInteractive",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
            ])
            .arg(&helper)
            .creation_flags(0x08000000);
        command
    };
    #[cfg(unix)]
    let mut command = {
        let mut command = Command::new("/bin/sh");
        command.arg(&helper);
        command
    };
    let ready = update.stage.join("ready");
    if ready.exists() {
        fs::remove_file(&ready).map_err(io_error)?;
    }
    let mut child = command
        .current_dir(&update.stage)
        .stdin(std::process::Stdio::null())
        .stdout(std::process::Stdio::null())
        .stderr(std::process::Stdio::null())
        .spawn()
        .map_err(io_error)?;
    let deadline = Instant::now() + Duration::from_secs(5);
    loop {
        if child.try_wait().map_err(io_error)?.is_some() {
            return Err(Failure(3, "Update helper could not start".into()));
        }
        if ready.is_file() {
            return Ok(());
        }
        if Instant::now() >= deadline {
            let _ = child.kill();
            let _ = child.wait();
            return Err(Failure(3, "Update helper did not become ready".into()));
        }
        std::thread::sleep(Duration::from_millis(20));
    }
}
