"""Give rolling nightly downloads stable names and regenerate their checksums."""
import argparse
import hashlib
from pathlib import Path


def prepare(directory: Path, version: str) -> None:
    prefix = f"BongoCat-X-{version}-"
    payloads = [p for p in directory.iterdir() if p.is_file() and p.suffix != ".sha256"]
    if not payloads:
        raise ValueError("No nightly payloads found")
    renames = []
    for payload in payloads:
        if not payload.name.startswith(prefix):
            raise ValueError(f"Unexpected nightly payload: {payload.name}")
        checksum = payload.with_name(payload.name + ".sha256")
        if not checksum.is_file():
            raise ValueError(f"Missing checksum for {payload.name}")
        target = payload.with_name("BongoCat-X-nightly-" + payload.name[len(prefix):])
        if target.exists() or target.with_name(target.name + ".sha256").exists():
            raise ValueError(f"Nightly target already exists: {target.name}")
        digest = hashlib.sha256(payload.read_bytes()).hexdigest()
        fields = checksum.read_text(encoding="utf-8-sig").split()
        if fields != [digest, payload.name]:
            raise ValueError(f"Invalid checksum for {payload.name}")
        renames.append((payload, checksum, target, digest))
    # Validate every payload before changing anything.
    for payload, checksum, target, digest in renames:
        payload.rename(target)
        target.with_name(target.name + ".sha256").write_text(
            f"{digest}  {target.name}\n", encoding="ascii")
        checksum.unlink()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, required=True)
    parser.add_argument("--version", required=True)
    args = parser.parse_args()
    prepare(args.directory, args.version)
