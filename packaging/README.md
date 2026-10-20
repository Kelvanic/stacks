# Desktop CI and releases

`.github/workflows/build-release.yml` builds, tests, packages and relocates the
application on pushes to `main`, pull requests, manual runs, and version tags.

| Target | Runner | Downloads |
|---|---|---|
| macOS Apple Silicon | macos-15 | DMG, app ZIP |
| macOS Intel | macos-15-intel | DMG, app ZIP |
| Windows x64 | windows-2022 / MSVC 2022 | per-user setup EXE, portable ZIP |
| Linux x64 | ubuntu-22.04 | AppImage, portable tar.gz, DEB |

These are four native builds, not a claim to support every historical OS or
architecture. macOS releases currently use Homebrew dependencies built for
macOS 15; Windows targets Windows 10/11; Linux targets glibc 2.35+ (Ubuntu 22.04+
and equivalent modern distributions). Linux ARM, Windows ARM and 32-bit builds
are not included.

## Publish a version

After the `main` build is green:

```bash
git tag v0.1.0
git push origin v0.1.0
```

A tag `vMAJOR.MINOR.PATCH` (optionally `-rc.1`, `-beta.1`, etc.) supplies the
application version, bundle/installer metadata, and download names.
`VERSION.txt` supplies the default for non-tag CI and local builds. Prerelease
tags publish prereleases. Manual runs build artifacts only unless the **publish**
checkbox is explicitly selected on `main`:

```bash
gh workflow run build-release.yml --ref main -f publish=true
```

That single run builds/tests all targets, then creates `v<VERSION.txt>` at the
exact tested commit and publishes the downloads. It refuses to reuse a version
tag pointing to a different commit. The tag is created with the workflow token,
so it does not recursively trigger another tag build.

The release job runs only when **all four** matrix jobs pass their tests and
package smoke checks. It verifies the nine expected packages and all individual
SHA256 files, emits a combined `SHA256SUMS`, uploads to a draft, then publishes.
An existing published release is never overwritten; drafts can be retried.
PR/build jobs only have read permission. Only the final tag-release job can
write repository releases. Actions and external binary dependencies are pinned
by commit/hash in the workflow and `tools/ci/dependencies.json`.

## What is bundled

QML, fonts, textures and artwork are embedded in the executable by default;
PDFium and sample books are installed alongside it. `macdeployqt`, `windeployqt`,
and `linuxdeploy --plugin qt` collect Qt libraries, QML imports, platform plugins,
SQL drivers and runtime dependencies. Windows also carries libzip/zlib and the
MSVC runtime. Linux DEB packages keep the bundled runtime under `/opt/stacks`,
not in the system's shared Qt directories. Notices and `BUILD_INFO.json` ship
with every package.

Smoke tests extract the actual archive into a fresh path containing spaces,
remove SDK/plugin environment paths, forbid checkout sample-data fallbacks,
then check the version, four sample books, library/EPUB/PDF screenshots and
search. They additionally exercise the AppImage runtime, verify DMG structure,
and install/launch/uninstall the Windows installer. The tests are headless
software-rendered checks, not a substitute for testing real GPU/audio devices
or every supported OS release.

Installer smoke tests pass NSIS's special `/D=` tail unquoted and last (as required
by NSIS, even for paths containing spaces), then require the app and uninstaller
at that exact destination. Each payload gets a fresh sample-library database.
DMG verification retries only the exact observed resource-unavailable diagnostic,
up to three times with 2s/5s delays; unknown/corrupt/changed images still fail.
ARM runners can leave a freshly created DMG attached to its creation helper.
Before calculating checksums, packaging matches only that artifact's canonical
path in `hdiutil info -plist`, selects its unique partition-map parent, and
normally detaches it. Other images are never detached, force flags are never
used, and ambiguous inventories or persistent failures stop packaging. Full
checksum verification still follows. Screenshots, installer paths, attachment
inventories and DMG attempt logs survive failed checks.

## Signing

No publisher credentials are required. macOS bundles use an ad-hoc signature,
not a Developer ID/notarized signature. Windows EXEs are unsigned. The release
notes disclose Gatekeeper/SmartScreen warnings. Do not claim these builds are
notarized or certificate-signed. Publisher signing can be added later using
protected GitHub environments/secrets; never commit certificate/private keys.

## Local packaging

Use Python 3.12+, build with `STACKS_BUNDLED_RESOURCES=ON`, and ensure Qt's deploy
tools are available. Select the matching target (`mac-arm64`, `mac-x64`,
`win-x64`, `linux-x64`):

```bash
python3 tools/ci/dependencies.py mac-arm64
# Configure normally with the matching PDFIUM_ROOT and Qt prefix, then:
cmake --build build --parallel
ctest --test-dir build --output-on-failure
python3 tools/ci/package.py --target mac-arm64 --version 0.1.0
python3 tools/ci/smoke.py --target mac-arm64 --version 0.1.0
```

Windows requires NSIS and `VCPKG_INSTALLED_DIR` pointing to the vcpkg installed
root. Linux requires patchelf, desktop-file-utils, dpkg-deb and the normal Qt
system dependencies. Downloads are placed in ignored `dist/`.
For live QML editing in a development checkout only, configure with
`-DSTACKS_BUNDLED_RESOURCES=OFF`; release jobs always embed resources.

To validate workflow/scripts locally:

```bash
actionlint .github/workflows/build-release.yml
python3 -m unittest discover -s tools/ci -p 'test_*.py' -v
```
