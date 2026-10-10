"""Regression checks for rolling nightly asset names and checksum integrity."""
import hashlib
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    "nightly_assets", Path(__file__).with_name("nightly-assets.py"))
assets = importlib.util.module_from_spec(spec)
spec.loader.exec_module(assets)


class NightlyAssetsTests(unittest.TestCase):
    def payload(self, root, version, platform, corrupt=False):
        path = root / f"BongoCat-X-{version}-{platform}"
        path.write_bytes(b"test package")
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        path.with_name(path.name + ".sha256").write_text(
            f"{digest if not corrupt else '0' * 64}  {path.name}\n", encoding="ascii")
        return path

    def test_versions_produce_the_same_names_and_valid_checksums(self):
        for version in ("2.1.1", "2.2.0"):
            with self.subTest(version=version), tempfile.TemporaryDirectory() as temp:
                root = Path(temp)
                for platform in ("windows-x64-setup.exe", "linux-x64.tar.gz",
                                 "linux-x64.AppImage", "macos-arm64.zip"):
                    self.payload(root, version, platform)
                assets.prepare(root, version)
                self.assertEqual(len(list(root.iterdir())), 8)
                for payload in root.iterdir():
                    self.assertTrue(payload.name.startswith("BongoCat-X-nightly-"))
                    if payload.suffix != ".sha256":
                        self.assertEqual(payload.read_bytes(), b"test package")
                        self.assertEqual(payload.with_name(payload.name + ".sha256")
                                         .read_text().split(),
                                         [hashlib.sha256(payload.read_bytes()).hexdigest(), payload.name])

    def test_invalid_checksum_does_not_rename_any_payload(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            good = self.payload(root, "2.1.1", "macos-arm64.zip")
            bad = self.payload(root, "2.1.1", "linux-x64.tar.gz", corrupt=True)
            with self.assertRaises(ValueError):
                assets.prepare(root, "2.1.1")
            self.assertTrue(good.exists() and bad.exists())
            self.assertFalse(list(root.glob("*nightly*")))

    def test_collision_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            self.payload(root, "2.1.1", "macos-x64.zip")
            (root / "BongoCat-X-nightly-macos-x64.zip").write_bytes(b"previous build")
            with self.assertRaises(ValueError):
                assets.prepare(root, "2.1.1")
            self.assertEqual((root / "BongoCat-X-nightly-macos-x64.zip").read_bytes(), b"previous build")


if __name__ == "__main__":
    unittest.main()
