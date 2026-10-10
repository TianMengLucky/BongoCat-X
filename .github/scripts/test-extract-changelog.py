"""Regression checks for release notes on Windows legacy stdout encodings."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


class ReleaseNotesTests(unittest.TestCase):
    def test_chinese_fallback_and_output_path_with_cp1252_stdout(self):
        script = Path(__file__).with_name("extract-changelog.py").resolve()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            changelog = root / "CHANGELOG.md"
            output = root / "发布说明.md"
            changelog.write_text("## [未发布]\n\n修复窗口裁剪。\n", encoding="utf-8")
            environment = dict(os.environ, PYTHONIOENCODING="cp1252:strict")
            result = subprocess.run(
                [sys.executable, str(script), "--tag", "nightly",
                 "--changelog", str(changelog), "--repository", "owner/repo",
                 "--artifact-version", "nightly", "--output", str(output)],
                env=environment, capture_output=True, check=False)
            self.assertEqual(result.returncode, 0, result.stderr.decode("ascii", "replace"))
            result.stdout.decode("ascii")
            notes = output.read_text(encoding="utf-8")
            self.assertIn("修复窗口裁剪。", notes)
            self.assertIn("Download (未发布)", notes)
            self.assertIn("/nightly/BongoCat-X-nightly-windows-x64-portable.zip", notes)


if __name__ == "__main__":
    unittest.main()
