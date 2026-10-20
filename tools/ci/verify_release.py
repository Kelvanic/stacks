"""Fail closed if any OS package or its checksum is missing/corrupt."""
import argparse
import hashlib
from pathlib import Path
from package import version_parts


def expected_assets(version):
    version_parts(version)
    variants = {"macos-arm64": ["zip", "dmg"], "macos-x86_64": ["zip", "dmg"],
                "windows-x86_64": ["zip", "setup.exe"], "linux-x86_64": ["AppImage", "tar.gz", "deb"]}
    result = set()
    for target, extensions in variants.items():
        for extension in extensions:
            separator = "-" if extension == "setup.exe" else "."
            result.add(f"Stacks-{version}-{target}{separator}{extension}")
    return result


def verify(directory, version):
    expected = expected_assets(version)
    actual = {p.name for p in directory.iterdir() if p.is_file() and not p.name.endswith(".sha256") and p.name != "SHA256SUMS"}
    if actual != expected:
        raise RuntimeError(f"Release asset mismatch: missing={expected - actual}, unexpected={actual - expected}")
    sums = []
    for name in sorted(expected):
        path = directory / name
        if path.stat().st_size == 0:
            raise RuntimeError(f"Empty release asset: {name}")
        digest = hashlib.sha256()
        with path.open("rb") as source:
            for chunk in iter(lambda: source.read(1024 * 1024), b""):
                digest.update(chunk)
        line = f"{digest.hexdigest()}  {name}\n"
        if path.with_name(name + ".sha256").read_text() != line:
            raise RuntimeError(f"Checksum mismatch for {name}")
        sums.append(line)
    (directory / "SHA256SUMS").write_text("".join(sums))
    print(f"Verified all {len(expected)} packages and their SHA256 checksums")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, required=True)
    parser.add_argument("--version", required=True)
    args = parser.parse_args()
    verify(args.directory, args.version)
