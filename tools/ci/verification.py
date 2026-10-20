"""Strict package-verification helpers, independent of the build host."""
import hashlib
import json
from pathlib import Path
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
