"""Pinned, checksum-verified build dependencies (Python 3.12+)."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tarfile
import time
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
LOCK = json.loads((Path(__file__).with_name("dependencies.json")).read_text())


def download(spec: dict, destination: Path) -> Path:
    destination.parent.mkdir(parents=True, exist_ok=True)
    expected = spec["sha256"]
    if destination.is_file():
        with destination.open("rb") as source:
            if hashlib.file_digest(source, "sha256").hexdigest() == expected:
                return destination
    partial = destination.with_suffix(destination.suffix + ".part")
    for attempt in range(3):
        try:
            request = urllib.request.Request(spec["url"], headers={"User-Agent": "Stacks-CI"})
            with urllib.request.urlopen(request, timeout=120) as response, partial.open("wb") as output:
                shutil.copyfileobj(response, output)
            with partial.open("rb") as source:
                actual = hashlib.file_digest(source, "sha256").hexdigest()
            if actual != expected:
                raise ValueError(f"SHA256 mismatch for {spec['url']}: {actual} != {expected}")
            partial.replace(destination)
            return destination
        except (OSError, ValueError):
            partial.unlink(missing_ok=True)
            if attempt == 2:
                raise
            time.sleep(2 ** attempt)
    raise AssertionError("unreachable")


def fetch_pdfium(target: str, destination: Path) -> None:
    spec = LOCK["pdfium"]["targets"][target]
    archive = download(spec, ROOT / "build-dependencies" / f"pdfium-{target}.tgz")
    destination.mkdir(parents=True, exist_ok=True)
    with tarfile.open(archive) as bundle:
        # Reject traversal, device files and links outside the destination.
        bundle.extractall(destination, filter="data")
    if not (destination / "include" / "fpdfview.h").is_file():
        raise RuntimeError("The PDFium archive has no expected include/fpdfview.h")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("target", choices=LOCK["pdfium"]["targets"])
    parser.add_argument("--destination", type=Path, default=ROOT / "third_party" / "pdfium-local")
    args = parser.parse_args()
    fetch_pdfium(args.target, args.destination)
