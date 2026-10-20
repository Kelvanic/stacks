import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from dependencies import LOCK, download
from package import version_parts, uninstall_commands, nsis_escape, is_macho
from smoke import isolated_environment
from verify_release import expected_assets, verify


class PackagingTests(unittest.TestCase):
    def test_version_validation(self):
        self.assertEqual(version_parts("1.2.3-rc.1"), "1.2.3")
        for bad in ["", "v1.2.3", "1.2", "1.2.3/evil", "1.2.3\n", "$(whoami)"]:
            with self.assertRaises(ValueError):
                version_parts(bad)

    def test_all_downloads_are_locked(self):
        specs = list(LOCK["pdfium"]["targets"].values()) + [LOCK["linuxdeploy"], LOCK["linuxdeploy_qt"]]
        for spec in specs:
            self.assertTrue(spec["url"].startswith("https://"))
            self.assertNotIn("/latest/", spec["url"])
            self.assertRegex(spec["sha256"], r"^[a-f0-9]{64}$")

    def test_bad_download_is_not_cached(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source"
            source.write_bytes(b"bad payload")
            destination = root / "download"
            spec = {"url": source.as_uri(), "sha256": "0" * 64}
            with patch("dependencies.time.sleep"), self.assertRaises(ValueError):
                download(spec, destination)
            self.assertFalse(destination.exists())
            spec["sha256"] = hashlib.sha256(source.read_bytes()).hexdigest()
            self.assertEqual(download(spec, destination).read_bytes(), b"bad payload")

    def test_uninstaller_never_recursively_deletes_user_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "resources").mkdir()
            (root / "resources" / "test.txt").write_text("test")
            commands = uninstall_commands(root)
            self.assertIn('Delete "$INSTDIR\\resources\\test.txt"', commands)
            self.assertIn('RMDir "$INSTDIR\\resources"', commands)
            self.assertNotIn("/r", commands)
        self.assertEqual(nsis_escape('$"'), '$$$\\"')

    def test_macho_detection_does_not_spawn_processes_for_qml_assets(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            binary = root / "library"
            binary.write_bytes(bytes.fromhex("cffaedfe") + b"binary")
            self.assertTrue(is_macho(binary))
            qml = root / "ui.qml"
            qml.write_text("import QtQuick")
            self.assertFalse(is_macho(qml))
            self.assertFalse(is_macho(root))

    def test_sdk_environment_is_removed(self):
        with patch.dict("os.environ", {"QT_PLUGIN_PATH": "fake-sdk", "LD_LIBRARY_PATH": "fake-sdk", "QML_IMPORT_PATH": "fake-sdk"}):
            env = isolated_environment()
        for key in ["QT_PLUGIN_PATH", "LD_LIBRARY_PATH", "QML_IMPORT_PATH"]:
            self.assertNotIn(key, env)
        self.assertEqual(env["STACKS_REQUIRE_BUNDLED_RESOURCES"], "1")

    def test_all_packages_required_before_release(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            expected = expected_assets("1.2.3")
            self.assertEqual(len(expected), 9)
            for name in expected:
                (root / name).write_bytes(b"package")
                (root / (name + ".sha256")).write_text(f"{hashlib.sha256(b'package').hexdigest()}  {name}\n")
            verify(root, "1.2.3")
            self.assertEqual(len((root / "SHA256SUMS").read_text().splitlines()), 9)
            victim = root / next(iter(expected))
            victim.write_bytes(b"corrupt")
            with self.assertRaises(RuntimeError):
                verify(root, "1.2.3")
            victim.unlink()
            with self.assertRaises(RuntimeError):
                verify(root, "1.2.3")


if __name__ == "__main__":
    unittest.main()
