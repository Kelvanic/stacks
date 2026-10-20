# Stacks desktop packages

A local-first reading desk for PDFs and EPUBs, with highlights, margin/voice notes,
full-text search, diagrams, and Send to Kindle.

## Choose a download

- **macOS Apple Silicon:** `macos-arm64.dmg` (or ZIP).
- **macOS Intel:** `macos-x86_64.dmg` (or ZIP).
- **Windows 10/11 x64:** `windows-x86_64-setup.exe` (per-user installer) or portable ZIP.
- **Linux x64:** AppImage, portable `.tar.gz`, or Debian/Ubuntu `.deb`.

macOS packages are ad-hoc signed, **not Apple-notarized**. Windows packages are
**unsigned**. Gatekeeper/SmartScreen may show publisher warnings; only proceed if
you trust this repository and have verified the download. No signing credentials
are included. Linux packages target Ubuntu 22.04/glibc 2.35 or newer; macOS builds
use macOS 15 runners (bundled Homebrew dependencies may require macOS 15).

All packages bundle the required Qt/QML and PDFium runtimes, fonts, artwork,
sample books, and dependency notices. Linux still requires a graphical session
and standard graphics/system libraries. Uninstalling does not delete your
personal library, notes, recordings, or credentials.

Verify downloads against `SHA256SUMS` (or the individual `.sha256` files).
Each package contains `BUILD_INFO.json` identifying its version, commit and
runtime dependencies. The release is published only after all four build/test
jobs and relocated-package launch checks succeed.
