"""Extract and launch the actual distribution with SDK environment removed."""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import tempfile
import zipfile
from dependencies import ROOT
from package import TARGETS, version_parts


def isolated_environment():
    env = os.environ.copy()
    for key in list(env):
        if key.startswith(("QML", "QT_", "DYLD_")) or key in {"LD_LIBRARY_PATH", "CMAKE_PREFIX_PATH"}:
            env.pop(key, None)
    if os.name == "nt":
        system = Path(env.get("SystemRoot", "C:/Windows"))
        env["PATH"] = os.pathsep.join(map(str, [system / "System32", system, system / "System32/Wbem"]))
    else:
        env["PATH"] = "/usr/bin:/bin:/usr/sbin:/sbin"
    env.update(QT_QPA_PLATFORM="offscreen", QT_QUICK_BACKEND="software", QT_QUICK_CONTROLS_STYLE="Basic",
               QT_FORCE_STDERR_LOGGING="1",
               STACKS_TEST_MODE="1", STACKS_REQUIRE_BUNDLED_RESOURCES="1", APPIMAGE_EXTRACT_AND_RUN="1")
    return env


def smoke(executable, work, version):
    env = isolated_environment()
    library = work / "test library"
    def invoke(*arguments):
        command = [str(executable), "--data-dir", str(library), *map(str, arguments)]
        result = subprocess.run(command, cwd=work, env=env, text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, timeout=90)
        print(result.stdout, flush=True)
        if result.returncode:
            raise RuntimeError(f"Packaged command failed ({result.returncode}): {command}")
        if re.search(r"ReferenceError|TypeError|module .* is not installed|Cannot open:|QQmlApplicationEngine failed", result.stdout):
            raise RuntimeError("Packaged UI reported missing resources or QML errors")
        return result.stdout
    if version not in invoke("--version"):
        raise RuntimeError("Packaged application reports the wrong version")
    result = invoke("--smoke-test", "--screenshot", work / "library.png")
    if "books=4" not in result:
        raise RuntimeError("The package did not load all four installed sample books")
    for book, name in [(2, "epub"), (3, "pdf")]:
        invoke("--open", book, "--demo", "--screenshot", work / f"{name}.png")
    result = invoke("--query", "margin")
    if "Systems That Breathe" not in result:
        raise RuntimeError("Search failed in the relocated package")
    for name in ["library", "epub", "pdf"]:
        path = work / f"{name}.png"
        if not path.is_file() or path.stat().st_size < 4000 or path.read_bytes()[:8] != b"\x89PNG\r\n\x1a\n":
            raise RuntimeError(f"Missing/empty packaged screenshot: {path}")


def test_package(out, target, version):
    version_parts(version)
    name = f"Stacks-{version}-{TARGETS[target]}"
    with tempfile.TemporaryDirectory(prefix="stacks-package-") as directory:
        work = Path(directory) / "Package With Spaces"
        work.mkdir()
        if target.startswith("mac"):
            subprocess.run(["ditto", "-x", "-k", str(out / f"{name}.zip"), str(work)], check=True)
            executable = work / "Stacks.app/Contents/MacOS/stacks"
        elif target.startswith("win"):
            with zipfile.ZipFile(out / f"{name}.zip") as archive:
                archive.extractall(work)
            executable = work / name / "stacks.exe"
        else:
            with tarfile.open(out / f"{name}.tar.gz") as archive:
                archive.extractall(work, filter="data")
            executable = work / name / "AppRun"
        if not executable.exists():
            raise RuntimeError(f"Package entry point is missing: {executable}")
        smoke(executable, work, version)
        if target.startswith("linux"):
            # The AppImage runtime is exercised as well as its portable tree.
            appimage = out / f"{name}.AppImage"
            appimage.chmod(0o755)
            smoke(appimage, work, version)
            subprocess.run(["dpkg-deb", "--info", str(out / f"{name}.deb")], check=True)
        elif target.startswith("mac"):
            subprocess.run(["hdiutil", "verify", str(out / f"{name}.dmg")], check=True)
        elif target.startswith("win"):
            # Also verify the actual installer payload, not just the ZIP.
            install = work / "Installed Stacks"
            process = subprocess.run([str(out / f"{name}-setup.exe"), "/S", f"/D={install}"], timeout=180)
            if process.returncode:
                raise RuntimeError("Silent installer failed")
            try:
                smoke(install / "stacks.exe", work, version)
            finally:
                subprocess.run([str(install / "Uninstall.exe"), "/S"], timeout=120, check=True)
        screenshots = ROOT / "build" / "package-smoke"
        screenshots.mkdir(parents=True, exist_ok=True)
        for png in work.glob("*.png"):
            shutil.copy2(png, screenshots)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=ROOT / "dist")
    parser.add_argument("--target", choices=TARGETS, required=True)
    parser.add_argument("--version", required=True)
    args = parser.parse_args()
    test_package(args.out.resolve(), args.target, args.version)
