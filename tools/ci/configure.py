"""Configure the same Release build on every GitHub runner."""
import argparse
import os
from pathlib import Path
import subprocess
from dependencies import ROOT
from package import version_parts

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", required=True)
    parser.add_argument("--version", required=True)
    args = parser.parse_args()
    command = ["cmake", "-S", str(ROOT), "-B", "build", "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
               "-DBUILD_TESTING=ON", "-DSTACKS_BUNDLED_RESOURCES=ON",
               f"-DSTACKS_VERSION={version_parts(args.version)}", f"-DSTACKS_DISPLAY_VERSION={args.version}",
               f"-DPDFIUM_ROOT={ROOT / 'third_party/pdfium-local'}", f"-DCMAKE_PREFIX_PATH={os.environ['QT_ROOT_DIR']}"]
    if args.target.startswith("mac"):
        command += [f"-DCMAKE_OSX_ARCHITECTURES={'arm64' if args.target.endswith('arm64') else 'x86_64'}",
                    "-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0"]
    elif args.target.startswith("win"):
        command += [f"-DCMAKE_TOOLCHAIN_FILE={Path(os.environ['VCPKG_ROOT']) / 'scripts/buildsystems/vcpkg.cmake'}",
                    "-DVCPKG_TARGET_TRIPLET=x64-windows"]
    subprocess.run(command, check=True)
