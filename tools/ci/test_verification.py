"""Regression coverage for the actual Windows/Apple Silicon CI failures."""
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import zipfile

from smoke import preserve_smoke_screenshots, test_package, smoke as app_smoke
from verification import nsis_install_command, verify_dmg, dmg_diagnostics

TRANSIENT = "hdiutil: verify failed - Resource temporarily unavailable\n"


class VerificationTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.image = self.root / "Disk Image With Spaces.dmg"
        self.image.write_bytes(b"unchanged image payload")
        self.logs = self.root / "diagnostics"

    def result(self, code=0, output="verified"):
        return subprocess.CompletedProcess([], code, stdout=output)

    def test_nsis_override_is_last_and_unquoted_even_with_spaces(self):
        installer = r"C:\Build Output\Stacks-setup.exe"
        directory = r"C:\Package With Spaces\Installed Stacks"
        command = nsis_install_command(installer, directory)
        self.assertEqual(command, f'"{installer}" /S /D={directory}')
        self.assertNotIn('"/D=', command)
        self.assertFalse(command.endswith('"'))
        with self.assertRaises(ValueError):
            nsis_install_command(installer, 'C:\\bad"path')

    def test_dmg_immediate_success(self):
        with patch("verification.subprocess.run", return_value=self.result()) as run, patch("verification.time.sleep") as sleep:
            verify_dmg(self.image, self.logs)
        self.assertEqual(run.call_count, 1)
        sleep.assert_not_called()
        self.assertEqual(run.call_args.args[0], ["hdiutil", "verify", str(self.image)])
        self.assertEqual(run.call_args.kwargs["timeout"], 120)
        self.assertEqual(run.call_args.kwargs["env"]["LC_ALL"], "C")
        self.assertTrue((self.logs / "dmg-verify-1.log").is_file())

    def test_dmg_transient_then_success_keeps_original_bytes(self):
        observed = f'hdiutil: verify: unable to recognize "{self.image}" as a disk image. (Resource temporarily unavailable)\n' + TRANSIENT
        with patch("verification.subprocess.run", side_effect=[self.result(1, observed), self.result()]) as run, patch("verification.time.sleep") as sleep, patch("verification.dmg_diagnostics") as diagnostics:
            verify_dmg(self.image, self.logs)
        self.assertEqual(run.call_count, 2)
        sleep.assert_called_once_with(2)
        diagnostics.assert_called_once_with(self.image, self.logs)
        self.assertEqual(self.image.read_bytes(), b"unchanged image payload")

    def test_dmg_transient_exhaustion_fails_closed(self):
        with patch("verification.subprocess.run", return_value=self.result(1, TRANSIENT)) as run, patch("verification.time.sleep") as sleep, patch("verification.dmg_diagnostics"):
            with self.assertRaises(subprocess.CalledProcessError):
                verify_dmg(self.image, self.logs)
        self.assertEqual(run.call_count, 3)
        self.assertEqual([call.args[0] for call in sleep.call_args_list], [2, 5])

    def test_dmg_unknown_and_corruption_failures_are_not_retried(self):
        for output in ["unrecognized image format", "permission denied", "checksum mismatch", TRANSIENT + "checksum mismatch",
                       "unknown failure\n" + TRANSIENT, "embedded " + TRANSIENT, TRANSIENT + "another error"]:
            with self.subTest(output=output), patch("verification.subprocess.run", return_value=self.result(1, output)) as run, patch("verification.time.sleep") as sleep:
                with self.assertRaises(subprocess.CalledProcessError):
                    verify_dmg(self.image, self.logs)
                self.assertEqual(run.call_count, 1)
                sleep.assert_not_called()

    def test_dmg_timeout_is_not_retried(self):
        with patch("verification.subprocess.run", side_effect=subprocess.TimeoutExpired("hdiutil", 120)) as run, patch("verification.time.sleep") as sleep:
            with self.assertRaises(subprocess.TimeoutExpired):
                verify_dmg(self.image, self.logs)
        self.assertEqual(run.call_count, 1)
        sleep.assert_not_called()

    def test_dmg_modified_during_verification_is_rejected(self):
        def modify(*args, **kwargs):
            self.image.write_bytes(b"changed")
            return self.result()
        with patch("verification.subprocess.run", side_effect=modify):
            with self.assertRaisesRegex(RuntimeError, "changed during"):
                verify_dmg(self.image, self.logs)

    def test_dmg_diagnostics_failure_does_not_mask_verification(self):
        with patch("verification.subprocess.run", side_effect=[self.result(1, TRANSIENT), self.result()]), patch("verification.time.sleep"), patch("verification.dmg_diagnostics", side_effect=OSError("diagnostics unavailable")):
            verify_dmg(self.image, self.logs)
        self.logs.mkdir(exist_ok=True)
        with patch("verification.subprocess.run", side_effect=FileNotFoundError("no lsof")):
            dmg_diagnostics(self.image, self.logs)
        self.assertIn("FileNotFoundError", (self.logs / "dmg-image-holders.log").read_text())

    def test_failed_smoke_preserves_screenshots(self):
        work = self.root / "work"
        work.mkdir()
        (work / "library.png").write_bytes(b"saved screenshot")
        with patch("smoke.ROOT", self.root):
            with self.assertRaisesRegex(RuntimeError, "original failure"):
                with preserve_smoke_screenshots(work):
                    raise RuntimeError("original failure")
        self.assertEqual((self.root / "build/package-smoke/library.png").read_bytes(), b"saved screenshot")

    def test_screenshot_copy_failure_does_not_mask_smoke_failure(self):
        work = self.root / "work"
        work.mkdir()
        (work / "library.png").write_bytes(b"screenshot")
        with patch("smoke.ROOT", self.root), patch("smoke.shutil.copy2", side_effect=OSError("copy failed")):
            with self.assertRaisesRegex(RuntimeError, "original failure"):
                with preserve_smoke_screenshots(work):
                    raise RuntimeError("original failure")

    def test_each_payload_gets_a_fresh_library(self):
        work = self.root / "work"
        work.mkdir()
        libraries = set()
        def invoke(command, **kwargs):
            libraries.add(command[command.index("--data-dir") + 1])
            if "--screenshot" in command:
                Path(command[command.index("--screenshot") + 1]).write_bytes(b"\x89PNG\r\n\x1a\n" + b"x" * 5000)
            output = "1.2.3\nbooks=4\nSystems That Breathe\n"
            return self.result(0, output)
        with patch("smoke.subprocess.run", side_effect=invoke):
            app_smoke(work / "portable/stacks.exe", work, "1.2.3")
            app_smoke(work / "installed/stacks.exe", work, "1.2.3")
        self.assertEqual(len(libraries), 2)

    def windows_archive(self):
        out = self.root / "dist"
        out.mkdir()
        with zipfile.ZipFile(out / "Stacks-1.2.3-windows-x86_64.zip", "w") as archive:
            archive.writestr("Stacks-1.2.3-windows-x86_64/stacks.exe", b"portable payload")
        return out

    def test_missing_installer_payload_does_not_launch_absent_uninstaller(self):
        out = self.windows_archive()
        with patch("smoke.ROOT", self.root), patch("smoke.smoke"), patch("smoke.subprocess.run", return_value=self.result()) as run:
            with self.assertRaisesRegex(RuntimeError, "expected app and uninstaller"):
                test_package(out, "win-x64", "1.2.3")
        self.assertEqual(run.call_count, 1)
        self.assertIsInstance(run.call_args.args[0], str)
        self.assertFalse(run.call_args.kwargs["shell"])
        self.assertTrue((self.root / "build/package-smoke/installer.json").is_file())

    def test_uninstall_error_does_not_mask_app_failure(self):
        out = self.windows_archive()
        def install_or_uninstall(command, **kwargs):
            if isinstance(command, str):
                destination = Path(command.split(" /D=", 1)[1])
                destination.mkdir()
                (destination / "stacks.exe").touch()
                (destination / "Uninstall.exe").touch()
                return self.result()
            raise OSError("cleanup failed")
        def smoke_result(executable, *args):
            if executable.parent.name == "Installed Stacks":
                raise RuntimeError("original app failure")
        original_write = Path.write_text
        for log_fails in [False, True]:
            def write(path, *args, **kwargs):
                if log_fails and path.name == "uninstall-error.log":
                    raise OSError("diagnostic write failed")
                return original_write(path, *args, **kwargs)
            with self.subTest(log_fails=log_fails), patch("smoke.ROOT", self.root), patch("smoke.smoke", side_effect=smoke_result), patch("smoke.subprocess.run", side_effect=install_or_uninstall), patch.object(Path, "write_text", new=write):
                with self.assertRaisesRegex(RuntimeError, "original app failure"):
                    test_package(out, "win-x64", "1.2.3")
        self.assertIn("cleanup failed", (self.root / "build/package-smoke/uninstall-error.log").read_text())


if __name__ == "__main__":
    unittest.main()
