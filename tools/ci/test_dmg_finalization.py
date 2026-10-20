"""A lingering creation attachment must never cause unrelated disk detaches."""
import hashlib
from pathlib import Path
import plistlib
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from verification import finalize_created_dmg
from package import package


class DmgFinalizationTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.image = self.root / "Owned Image.dmg"
        self.image.write_bytes(b"created artifact")
        self.logs = self.root / "logs"
        self.unrelated = {"image-path": "/System/Library/Unrelated.dmg", "system-entities": [
            {"dev-entry": "/dev/disk1", "content-hint": "GUID_partition_scheme"}]}

    def record(self, parent=5, synthesized=6):
        return {"image-path": str(self.image), "system-entities": [
            {"dev-entry": f"/dev/disk{parent}", "content-hint": "GUID_partition_scheme"},
            {"dev-entry": f"/dev/disk{parent}s1", "content-hint": "Apple_APFS"},
            {"dev-entry": f"/dev/disk{synthesized}", "content-hint": "Apple_APFS"},
            {"dev-entry": f"/dev/disk{synthesized}s1", "content-hint": "Apple_APFS"}]}

    def info(self, records):
        return subprocess.CompletedProcess([], 0, stdout=plistlib.dumps({"images": records}))

    def test_unattached_artifact_never_detaches_other_images(self):
        with patch("verification.subprocess.run", return_value=self.info([self.unrelated])) as run:
            finalize_created_dmg(self.image, self.logs)
        self.assertEqual(run.call_count, 1)
        self.assertEqual(run.call_args.args[0], ["hdiutil", "info", "-plist"])

    def test_detaches_only_exact_artifact_partition_parent(self):
        sequence = [self.info([self.unrelated, self.record()]), subprocess.CompletedProcess([], 0, stdout="disk5 ejected"), self.info([self.unrelated])]
        with patch("verification.subprocess.run", side_effect=sequence) as run:
            finalize_created_dmg(self.image, self.logs)
        self.assertEqual(run.call_args_list[1].args[0], ["hdiutil", "detach", "/dev/disk5"])
        self.assertTrue((self.logs / "dmg-finalize-1.json").is_file())
        self.assertEqual(self.image.read_bytes(), b"created artifact")

    def test_reordered_entities_and_canonical_path_select_the_parent(self):
        record = self.record()
        record["system-entities"].reverse()
        alias = self.root / "alias"
        alias.mkdir()
        record["image-path"] = str(alias / ".." / self.image.name)
        sequence = [self.info([record]), subprocess.CompletedProcess([], 0, stdout="ejected"), self.info([])]
        with patch("verification.subprocess.run", side_effect=sequence) as run:
            finalize_created_dmg(self.image, self.logs)
        self.assertEqual(run.call_args_list[1].args[0], ["hdiutil", "detach", "/dev/disk5"])

    def test_same_name_in_different_directory_is_not_ours(self):
        other = self.record()
        other["image-path"] = str(self.root / "other" / self.image.name)
        with patch("verification.subprocess.run", return_value=self.info([other])) as run:
            finalize_created_dmg(self.image, self.logs)
        self.assertEqual(run.call_count, 1)

    def test_ambiguous_topology_and_duplicate_attachments_fail_without_detach(self):
        cases = []
        missing = self.record(); missing["system-entities"] = []
        cases.append([missing])
        ambiguous = self.record()
        ambiguous["system-entities"][0]["content-hint"] = "unknown"
        cases.append([ambiguous])
        single_unknown = self.record()
        single_unknown["system-entities"] = [{"dev-entry": "/dev/disk5", "content-hint": "unknown"}]
        cases.append([single_unknown])
        multiple_parents = self.record()
        multiple_parents["system-entities"][2]["content-hint"] = "GUID_partition_scheme"
        cases.append([multiple_parents])
        cases.append([self.record(), self.record(7, 8)])
        cases.append([{"image-path": "relative/path.dmg"}])
        for records in cases:
            with self.subTest(records=records), patch("verification.subprocess.run", return_value=self.info(records)) as run:
                with self.assertRaises(RuntimeError):
                    finalize_created_dmg(self.image, self.logs)
                self.assertEqual(run.call_count, 1)

    def test_busy_detach_is_bounded_and_requeries_device_identity(self):
        sequence = [self.info([self.record()]), subprocess.CompletedProcess([], 1, stdout="Resource busy"),
                    self.info([self.record()]), self.info([self.unrelated, self.record(7, 8)]),
                    subprocess.CompletedProcess([], 0, stdout="disk7 ejected"), self.info([self.unrelated])]
        with patch("verification.subprocess.run", side_effect=sequence) as run, patch("verification.time.sleep") as sleep:
            finalize_created_dmg(self.image, self.logs)
        commands = [call.args[0] for call in run.call_args_list if call.args[0][1] == "detach"]
        self.assertEqual(commands, [["hdiutil", "detach", "/dev/disk5"], ["hdiutil", "detach", "/dev/disk7"]])
        sleep.assert_called_once_with(2)

    def test_persistent_busy_attachment_fails_without_force(self):
        def invoke(command, **kwargs):
            if command[1] == "info":
                return self.info([self.unrelated, self.record()])
            return subprocess.CompletedProcess([], 1, stdout="Resource busy")
        with patch("verification.subprocess.run", side_effect=invoke) as run, patch("verification.time.sleep") as sleep:
            with self.assertRaisesRegex(RuntimeError, "remains attached"):
                finalize_created_dmg(self.image, self.logs)
        commands = [call.args[0] for call in run.call_args_list if call.args[0][1] == "detach"]
        self.assertEqual(commands, [["hdiutil", "detach", "/dev/disk5"]] * 3)
        self.assertEqual([call.args[0] for call in sleep.call_args_list], [2, 5])

    def test_unknown_detach_failure_is_not_retried(self):
        sequence = [self.info([self.record()]), subprocess.CompletedProcess([], 1, stdout="permission denied"), self.info([self.record()])]
        with patch("verification.subprocess.run", side_effect=sequence) as run, patch("verification.time.sleep") as sleep:
            with self.assertRaises(subprocess.CalledProcessError):
                finalize_created_dmg(self.image, self.logs)
        self.assertEqual(run.call_count, 3)
        sleep.assert_not_called()

    def test_timeouts_fail_closed_and_preserve_attempt_evidence(self):
        with patch("verification.subprocess.run", side_effect=subprocess.TimeoutExpired("hdiutil info", 30)) as run:
            with self.assertRaises(subprocess.TimeoutExpired):
                finalize_created_dmg(self.image, self.logs)
        self.assertEqual(run.call_count, 1)
        self.assertTrue((self.logs / "dmg-inventory-1.log").is_file())
        sequence = [self.info([self.record()]), subprocess.TimeoutExpired("hdiutil detach", 60)]
        with patch("verification.subprocess.run", side_effect=sequence):
            with self.assertRaises(subprocess.TimeoutExpired):
                finalize_created_dmg(self.image, self.logs)
        self.assertTrue((self.logs / "dmg-finalize-1.json").is_file())

    def test_checksums_cover_bytes_after_created_image_finalization(self):
        qt = self.root / "qt"
        plugin = qt / "plugins/platforms/libqoffscreen.dylib"
        plugin.parent.mkdir(parents=True)
        plugin.write_bytes(b"dummy plugin")
        out = self.root / "dist"
        (self.root / "build").mkdir()
        def run(*command, **kwargs):
            if str(command[0]) == "cmake":
                (Path(command[-1]) / "stacks.app").mkdir(parents=True)
            elif str(command[0]) == "ditto":
                Path(command[-1]).write_bytes(b"zip payload")
            elif str(command[0]) == "hdiutil":
                Path(command[-1]).write_bytes(b"unfinished DMG")
            return subprocess.CompletedProcess(command, 0)
        def finalize(image, logs):
            self.assertEqual(image.read_bytes(), b"unfinished DMG")
            image.write_bytes(b"finalized DMG")
        with patch("package.run", side_effect=run), patch("package.qt_prefix", return_value=qt), patch("package.write_build_info"), patch("package.finalize_created_dmg", side_effect=finalize) as finalizer:
            package(self.root / "build", out, "mac-arm64", "1.2.3")
        finalizer.assert_called_once()
        image = out / "Stacks-1.2.3-macos-arm64.dmg"
        expected = hashlib.sha256(b"finalized DMG").hexdigest()
        self.assertEqual(image.with_name(image.name + ".sha256").read_text(), f"{expected}  {image.name}\n")

    def test_invalid_inventory_cannot_authorize_detach(self):
        for payload in [b"invalid plist", plistlib.dumps({"images": "not a list"})]:
            with self.subTest(payload=payload), patch("verification.subprocess.run", return_value=subprocess.CompletedProcess([], 0, stdout=payload)) as run:
                with self.assertRaises((RuntimeError, plistlib.InvalidFileException)):
                    finalize_created_dmg(self.image, self.logs)
                self.assertEqual(run.call_count, 1)


if __name__ == "__main__":
    unittest.main()
