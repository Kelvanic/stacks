"""Strict package-verification helpers, independent of the build host."""
import hashlib
import json
from pathlib import Path
import plistlib
import re
import subprocess
import os
import time


def nsis_install_command(installer, destination):
    # NSIS parses /D itself, not through the normal Windows argv rules. It must
    # be the final, unquoted command-line tail, INCLUDING when it contains spaces.
    # Pass this directly to CreateProcess (shell=False), never to cmd.exe.
    # https://nsis.sourceforge.io/Docs/Chapter3.html#installerusage
    destination = str(destination)
    if any(character in destination for character in ('"', '\r', '\n', '\0')):
        raise ValueError("Invalid NSIS install destination")
    return subprocess.list2cmdline([str(installer), "/S"]) + " /D=" + destination


def file_identity(path):
    with path.open("rb") as source:
        digest = hashlib.file_digest(source, "sha256").hexdigest()
    stat = path.stat()
    return {"size": stat.st_size, "mtime_ns": stat.st_mtime_ns, "sha256": digest}


def dmg_diagnostics(image, directory):
    # Best-effort observation only: never detach volumes, disable Spotlight or
    # kill system processes on a runner merely to make verification succeed.
    for name, command in [("disk-images", ["hdiutil", "info"]), ("image-holders", ["lsof", str(image)])]:
        try:
            result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                    text=True, errors="replace", timeout=10)
            output = f"exit={result.returncode}\n{result.stdout}"
        except (OSError, subprocess.TimeoutExpired) as error:
            output = repr(error)
        (directory / f"dmg-{name}.log").write_text(output, encoding="utf-8")


def created_image_record(image, info):
    """Find only this freshly-created artifact, never an unrelated mount."""
    entries = info.get("images", [])
    if any(not isinstance(entry, dict) or not isinstance(entry.get("image-path"), str)
           or not Path(entry["image-path"]).is_absolute() for entry in entries):
        raise RuntimeError("Malformed disk-image attachment path")
    records = [entry for entry in entries if Path(entry["image-path"]).resolve() == image]
    if len(records) > 1:
        raise RuntimeError("Created DMG has ambiguous multiple attachments")
    return records[0] if records else None


def created_image_device(record):
    # APFS images include a second synthesized whole disk. Prefer the unique
    # partition-map parent; never guess between multiple whole-device entries.
    entities = record.get("system-entities", record.get("dev-entries", []))
    whole = [entry for entry in entities if re.fullmatch(r"/dev/disk[0-9]+", entry.get("dev-entry", ""))]
    parents = [entry for entry in whole if entry.get("content-hint") in
               {"GUID_partition_scheme", "FDisk_partition_scheme", "Apple_partition_scheme"}]
    if len(parents) == 1:
        return parents[0]["dev-entry"]
    raise RuntimeError("Cannot identify a unique whole device for the created DMG")


def finalize_created_dmg(image, directory):
    """Detach only our own lingering creation attachment, before hashing it."""
    image, directory = Path(image).resolve(strict=True), Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    inventory_count = 0
    def image_info():
        nonlocal inventory_count
        inventory_count += 1
        log = directory / f"dmg-inventory-{inventory_count}"
        try:
            result = subprocess.run(["hdiutil", "info", "-plist"], check=True,
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
        except (OSError, subprocess.SubprocessError) as error:
            log.with_suffix(".log").write_text(repr(error), encoding="utf-8")
            raise
        log.with_suffix(".plist").write_bytes(result.stdout)
        info = plistlib.loads(result.stdout)
        if not isinstance(info, dict) or not isinstance(info.get("images"), list):
            raise RuntimeError("Malformed disk-image attachment inventory")
        return info
    for attempt in range(3):
        record = created_image_record(image, image_info())
        if record is None:
            return
        device = created_image_device(record)
        print(f"Finalizing our created DMG: detach {device} ({image})", flush=True)
        command = ["hdiutil", "detach", device] # Deliberately no force flag.
        try:
            result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                    text=True, errors="replace", timeout=60)
        except (OSError, subprocess.SubprocessError) as error:
            (directory / f"dmg-finalize-{attempt + 1}.json").write_text(
                json.dumps({"image": str(image), "device": device, "attachment": record,
                            "error": repr(error)}, indent=2, default=str), encoding="utf-8")
            raise
        (directory / f"dmg-finalize-{attempt + 1}.json").write_text(
            json.dumps({"image": str(image), "device": device, "attachment": record,
                        "exit_code": result.returncode, "output": result.stdout}, indent=2, default=str), encoding="utf-8")
        # Re-query the artifact, not the old device number: a disappearing helper
        # or a reused disk number must not authorize detaching another image.
        if created_image_record(image, image_info()) is None:
            return
        if result.returncode and not any(term in (result.stdout or "").lower()
                                          for term in ("resource busy", "resource temporarily unavailable")):
            raise subprocess.CalledProcessError(result.returncode, command, output=result.stdout)
        if attempt == 2:
            raise RuntimeError("Created DMG remains attached after bounded ordinary detach attempts")
        time.sleep((2, 5)[attempt])
    raise AssertionError("unreachable")


def verify_dmg(image, directory):
    image, directory = Path(image), Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    original = file_identity(image)
    (directory / "dmg-identity.json").write_text(json.dumps({"path": str(image), **original}, indent=2), encoding="utf-8")
    env = os.environ.copy()
    env.update(LC_ALL="C", LANG="C")
    command = ["hdiutil", "verify", str(image)]
    delays = (2, 5)
    for attempt in range(3):
        if file_identity(image) != original:
            raise RuntimeError("DMG changed between verification attempts")
        started = time.monotonic()
        log = directory / f"dmg-verify-{attempt + 1}.log"
        try:
            result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                    text=True, errors="replace", timeout=120)
        except (OSError, subprocess.TimeoutExpired) as error:
            log.write_text(repr(error), encoding="utf-8")
            raise
        output = result.stdout or ""
        log.write_text(f"attempt={attempt + 1} exit={result.returncode} elapsed={time.monotonic() - started:.2f}s\n{output}", encoding="utf-8")
        print(log.read_text(encoding="utf-8"), flush=True)
        if file_identity(image) != original:
            raise RuntimeError("DMG changed during verification")
        if result.returncode == 0:
            return
        normalized = output.lower()
        corrupt = any(term in normalized for term in ("checksum mismatch", "checksum failed", "invalid checksum", "corrupt"))
        lines = [line.strip() for line in normalized.splitlines() if line.strip()]
        terminal = "hdiutil: verify failed - resource temporarily unavailable"
        recognized = f'hdiutil: verify: unable to recognize "{str(image).lower()}" as a disk image. (resource temporarily unavailable)'
        # Only the exact observed terminal error, optionally preceded by its
        # matching recognition diagnostic, is retryable. Mixed/unknown errors
        # must not be superseded by a later successful attempt.
        transient = bool(lines) and lines[-1] == terminal and all(line in {terminal, recognized} for line in lines)
        if attempt == 0 and transient and not corrupt:
            try:
                dmg_diagnostics(image, directory)
            except Exception as error:
                print(f"Additional DMG diagnostics unavailable: {error}", flush=True)
        if corrupt or not transient or attempt == 2:
            raise subprocess.CalledProcessError(result.returncode, command, output=output)
        time.sleep(delays[attempt])
    raise AssertionError("unreachable")
