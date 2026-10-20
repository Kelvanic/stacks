"""Create relocatable desktop packages after building/testing with CMake."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import zipfile

from dependencies import ROOT, LOCK, download
from verification import finalize_created_dmg

TARGETS = {
    "mac-arm64": "macos-arm64",
    "mac-x64": "macos-x86_64",
    "win-x64": "windows-x86_64",
    "linux-x64": "linux-x86_64",
}


def run(*args, **kwargs):
    print("+", " ".join(map(str, args)), flush=True)
    return subprocess.run(list(map(str, args)), check=True, **kwargs)


def version_parts(value):
    match = re.fullmatch(r"(\d+\.\d+\.\d+)(?:-[0-9A-Za-z][0-9A-Za-z.-]*)?", value)
    if not match:
        raise ValueError(f"Not a supported release version: {value!r}")
    return match.group(1)


def nsis_escape(value):
    return str(value).replace("$", "$$").replace('"', '$\\"')


def uninstall_commands(stage):
    files = sorted(p for p in stage.rglob("*") if p.is_file())
    directories = sorted((p for p in stage.rglob("*") if p.is_dir()), key=lambda p: len(p.parts), reverse=True)
    lines = [f'    Delete "$INSTDIR\\{nsis_escape(p.relative_to(stage)).replace("/", chr(92))}"' for p in files]
    lines += [f'    RMDir "$INSTDIR\\{nsis_escape(p.relative_to(stage)).replace("/", chr(92))}"' for p in directories]
    return "\n".join(lines)


def qt_prefix():
    if os.environ.get("QT_ROOT_DIR"):
        return Path(os.environ["QT_ROOT_DIR"])
    return Path(subprocess.check_output(["qmake", "-query", "QT_INSTALL_PREFIX"], text=True).strip())


def write_build_info(data_dir, target, version, qt):
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    qt_version = subprocess.check_output([str(qt / "bin" / ("qmake.exe" if os.name == "nt" else "qmake")), "-query", "QT_VERSION"], text=True).strip()
    info = {"version": version, "commit": commit, "target": target, "qt": qt_version,
            "pdfium": LOCK["pdfium"]["release"], "signed_by_publisher": False}
    (data_dir / "BUILD_INFO.json").write_text(json.dumps(info, indent=2) + "\n", encoding="utf-8")


def is_macho(path):
    if path.is_symlink() or not path.is_file():
        return False
    with path.open("rb") as source:
        magic = source.read(4)
    return magic in {bytes.fromhex(value) for value in ("feedface", "feedfacf", "cefaedfe", "cffaedfe", "cafebabe", "bebafeca", "cafebabf", "bfbafeca")}


def macho_dependencies(output):
    # Universal binaries have a separate non-indented heading per architecture.
    return [line.strip().split(" (", 1)[0] for line in output.splitlines() if line[:1].isspace()]


def mac_package(stage, out, name, qt):
    app = stage / "stacks.app"
    # Native deploy tools normally include only cocoa/windows. Include the
    # offscreen platform explicitly for the shipped CLI/screenshot mode too.
    offscreen = app / "Contents/PlugIns/platforms/libqoffscreen.dylib"
    offscreen.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(qt / "plugins/platforms/libqoffscreen.dylib", offscreen)
    run(qt / "bin" / "macdeployqt", app, f"-qmldir={ROOT / 'qml'}", f"-executable={offscreen}", "-always-overwrite", "-no-strip", "-verbose=2")
    # Only SQLite is used. SDK builds can carry optional ODBC/Postgres/Mimer
    # drivers pointing to databases on the Qt build machine, which must not ship.
    sql_drivers = app / "Contents/PlugIns/sqldrivers"
    for driver in sql_drivers.glob("*.dylib"):
        if driver.name != "libqsqlite.dylib":
            driver.unlink()
    binaries = []
    replacements = {}
    for path in app.rglob("*"):
        if is_macho(path):
            binaries.append(path)
            if path.parent == app / "Contents/Frameworks" and path.suffix == ".dylib":
                identity = subprocess.check_output(["otool", "-D", str(path)], text=True).splitlines()
                new_id = f"@rpath/{path.name}"
                replacements[str(path)] = new_id
                if len(identity) > 1:
                    replacements[identity[1].strip()] = new_id
                run("install_name_tool", "-id", new_id, path)
    for path in binaries:
        deps = macho_dependencies(subprocess.check_output(["otool", "-L", str(path)], text=True))
        for old in deps:
            if old in replacements and old != replacements[old]:
                run("install_name_tool", "-change", old, replacements[old], path)
    app = app.rename(stage / "Stacks.app")
    run("codesign", "--force", "--deep", "--sign", "-", app)
    run("codesign", "--verify", "--deep", "--strict", app)
    # Fail if any bundled binary still loads a dependency from Homebrew/the SDK.
    for path in app.rglob("*"):
        if not is_macho(path):
            continue
        identity = subprocess.check_output(["otool", "-D", str(path)], text=True).splitlines()[1:]
        deps = macho_dependencies(subprocess.check_output(["otool", "-L", str(path)], text=True))
        for dependency in deps:
            if dependency in identity: # A dylib's own ID is not a loaded dependency.
                continue
            if dependency.startswith("/") and not dependency.startswith(("/System/Library/", "/usr/lib/")):
                raise RuntimeError(f"Non-relocatable dependency in {path}: {dependency}")
    run("ditto", "-c", "-k", "--sequesterRsrc", "--keepParent", app, out / f"{name}.zip")
    dmg_root = stage / "dmg"
    dmg_root.mkdir()
    shutil.copytree(app, dmg_root / app.name, symlinks=True)
    (dmg_root / "Applications").symlink_to("/Applications")
    image = out / f"{name}.dmg"
    run("hdiutil", "create", "-volname", "Stacks", "-srcfolder", dmg_root, "-format", "UDZO", image)
    # ARM runners can leave the creation device attached after hdiutil exits.
    # Finalize that exact owned image before package() computes SHA256 files.
    finalize_created_dmg(image, ROOT / "build/package-smoke")


def windows_package(stage, out, name, qt, version):
    vcpkg = Path(os.environ["VCPKG_INSTALLED_DIR"])
    for dll in (vcpkg / "x64-windows" / "bin").glob("*.dll"):
        shutil.copy2(dll, stage)
    copyrights = vcpkg / "x64-windows" / "share"
    for copyright_file in copyrights.glob("*/copyright"):
        destination = stage / "resources" / "licenses" / "vcpkg" / copyright_file.parent.name
        destination.mkdir(parents=True, exist_ok=True)
        shutil.copy2(copyright_file, destination)
    offscreen = stage / "platforms/qoffscreen.dll"
    offscreen.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(qt / "plugins/platforms/qoffscreen.dll", offscreen)
    run(qt / "bin" / "windeployqt.exe", "--release", "--compiler-runtime", "--qmldir", ROOT / "qml", "--dir", stage, stage / "stacks.exe", offscreen)
    # windeployqt can copy only the redistributable installer. Portable ZIPs
    # also need the actual redistributable CRT DLLs, not a runner-global runtime.
    runtime = Path(os.environ["VCToolsRedistDir"]) / "x64/Microsoft.VC143.CRT"
    crt_dlls = list(runtime.glob("*.dll"))
    if not crt_dlls:
        raise RuntimeError(f"MSVC redistributable DLLs are missing: {runtime}")
    for dll in crt_dlls:
        shutil.copy2(dll, stage)
    # Qt/SDK tools may otherwise leave absolute developer paths in their config.
    (stage / "qt.conf").write_text("[Paths]\nPrefix=.\nPlugins=.\nQmlImports=qml\n", encoding="utf-8")
    shutil.make_archive(str(out / name), "zip", stage.parent, stage.name)
    template = (ROOT / "packaging" / "windows.nsi").read_text()
    for key, value in {"STAGE": stage, "OUTPUT": out / f"{name}-setup.exe", "ICON": ROOT / "assets/branding/Stacks.ico", "VERSION": version}.items():
        template = template.replace(f"@{key}@", nsis_escape(value))
    template = template.replace("@UNINSTALL_FILES@", uninstall_commands(stage))
    script = stage.parent / "installer.nsi"
    script.write_text(template, encoding="utf-8")
    makensis = shutil.which("makensis") or str(Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "NSIS/makensis.exe")
    run(makensis, "/V3", script)


def linux_qmake_overlay(qt, work):
    # linuxdeploy's SQL deployer has no driver filter and otherwise tries to
    # bundle optional Mimer/Oracle/Postgres client libraries. Present a temporary
    # plugin view containing only SQLite, without changing the cached Qt SDK.
    plugins = work / "qt-plugins"
    plugins.mkdir()
    for source in (qt / "plugins").iterdir():
        if source.name != "sqldrivers":
            (plugins / source.name).symlink_to(source, target_is_directory=source.is_dir())
    (plugins / "sqldrivers").mkdir()
    (plugins / "sqldrivers/libqsqlite.so").symlink_to(qt / "plugins/sqldrivers/libqsqlite.so")
    wrapper = work / "qmake-for-deployment"
    wrapper.write_text(
        "#!/usr/bin/env python3\nimport subprocess,sys\n"
        f"real = {str(qt / 'bin/qmake')!r}\nplugins = {str(plugins)!r}\n"
        "if sys.argv[1:] == ['-query', 'QT_INSTALL_PLUGINS']:\n    print(plugins)\n"
        "elif sys.argv[1:] == ['-query']:\n"
        "    output = subprocess.check_output([real, '-query'], text=True)\n"
        "    print('\\n'.join('QT_INSTALL_PLUGINS:' + plugins if line.startswith('QT_INSTALL_PLUGINS:') else line for line in output.splitlines()))\n"
        "else:\n    sys.exit(subprocess.call([real, *sys.argv[1:]]))\n")
    wrapper.chmod(0o755)
    return wrapper


def linux_package(stage, out, name, qt, version):
    tools = ROOT / "build-dependencies"
    deploy = download(LOCK["linuxdeploy"], tools / "linuxdeploy-x86_64.AppImage")
    plugin = download(LOCK["linuxdeploy_qt"], tools / "linuxdeploy-plugin-qt-x86_64.AppImage")
    deploy.chmod(0o755)
    plugin.chmod(0o755)
    env = os.environ.copy()
    qmake = linux_qmake_overlay(qt, stage.parent)
    env.update(APPIMAGE_EXTRACT_AND_RUN="1", QMAKE=str(qmake),
               QML_SOURCES_PATHS=str(ROOT / "qml"), EXTRA_PLATFORM_PLUGINS="libqoffscreen.so",
               NO_STRIP="1", ARCH="x86_64", VERSION=version, OUTPUT=str(out / f"{name}.AppImage"))
    run(deploy, "--appdir", stage, "--plugin", "qt", "--output", "appimage", env=env)
    if not (out / f"{name}.AppImage").is_file():
        raise RuntimeError("linuxdeploy did not produce the requested AppImage")
    with tarfile.open(out / f"{name}.tar.gz", "w:gz") as archive:
        archive.add(stage, arcname=name)
    # Keep private Qt libraries out of /usr/lib, to avoid affecting other apps.
    deb = stage.parent / "deb"
    private = deb / "opt" / "stacks"
    shutil.copytree(stage, private, symlinks=True)
    (deb / "usr/bin").mkdir(parents=True)
    launcher = deb / "usr/bin/stacks"
    launcher.write_text('#!/bin/sh\nexec /opt/stacks/AppRun "$@"\n')
    launcher.chmod(0o755)
    for relative in ["share/applications/stacks.desktop", "share/icons/hicolor/1024x1024/apps/stacks.png"]:
        destination = deb / "usr" / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(stage / "usr" / relative, destination)
    control = deb / "DEBIAN"
    control.mkdir()
    deb_version = version.replace("-", "~", 1)
    (control / "control").write_text(
        f"Package: stacks\nVersion: {deb_version}\nArchitecture: amd64\n"
        "Maintainer: Stacks maintainers <noreply@github.com>\nSection: utils\nPriority: optional\n"
        "Depends: libc6 (>= 2.35), libstdc++6, libgl1, libegl1, libx11-6, libxcb1, libxkbcommon0, libfontconfig1, libdbus-1-3\n"
        "Homepage: https://github.com/Kelvanic/stacks\nDescription: Local-first PDF and EPUB reading desk\n"
        " Includes the Qt/QML runtime in a private application directory.\n")
    run("dpkg-deb", "--root-owner-group", "--build", deb, out / f"{name}.deb")


def package(build, out, target, version):
    version_parts(version)
    build, out = build.resolve(), out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    work = build / "package-work"
    if work.exists():
        shutil.rmtree(work)
    work.mkdir()
    name = f"Stacks-{version}-{TARGETS[target]}"
    stage = work / ("AppDir" if target.startswith("linux") else name)
    prefix = stage / "usr" if target.startswith("linux") else stage
    run("cmake", "--install", build, "--config", "Release", "--prefix", prefix)
    qt = qt_prefix()
    if target.startswith("mac"):
        data_dir = stage / "stacks.app/Contents/Resources"
    elif target.startswith("win"):
        data_dir = stage / "resources"
    else:
        data_dir = stage / "usr/share/stacks"
    write_build_info(data_dir, target, version, qt)
    if target.startswith("mac"):
        mac_package(stage, out, name, qt)
    elif target.startswith("win"):
        windows_package(stage, out, name, qt, version)
    else:
        linux_package(stage, out, name, qt, version)
    assets = sorted(p for p in out.glob(f"{name}*") if p.is_file() and not p.name.endswith(".sha256"))
    if not assets:
        raise RuntimeError("No packages were created")
    for asset in assets:
        with asset.open("rb") as source:
            digest = hashlib.file_digest(source, "sha256").hexdigest()
        asset.with_name(asset.name + ".sha256").write_text(f"{digest}  {asset.name}\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=ROOT / "build")
    parser.add_argument("--out", type=Path, default=ROOT / "dist")
    parser.add_argument("--target", choices=TARGETS, required=True)
    parser.add_argument("--version", required=True)
    args = parser.parse_args()
    package(args.build, args.out, args.target, args.version)
