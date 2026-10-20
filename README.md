# Stacks

A local-first reading desk for macOS, Windows, and Linux. Built with **Qt 6 + QML + C++20**, **PDFium** for PDF, **QTextDocument** for reflowable EPUB, **SQLite FTS5** for search, and native audio capture for voice notes.

This is the working prototype behind `/root/ideas/stacks/prd.md` and `/root/ideas/stacks/design.md`.

---

## What works today

| Feature | Status |
|---|---|
| Import PDF and EPUB | Yes |
| Library with continue-reading progress | Yes |
| Reading-position memory | Yes, per book, persisted in SQLite |
| PDF rendering through PDFium | Yes, lazy page render + page cache |
| EPUB rendering through QTextDocument | Yes, continuous scroll, text selection |
| Highlights | EPUB text ranges and PDF rectangular regions |
| Bookmarks | Yes |
| Margin notes | Yes |
| Voice notes | Yes, `QAudioSource` → WAV, attach as a margin entry |
| Full-text search | SQLite FTS5 with snippets, ranked results, jump-to-passage |
| Diagram canvas | Node/edge canvas linked to the current book |
| Command palette | `⌘K` / `Ctrl+K` global search, grouped hits, jump-to-passage |
| File-to-Kindle converter | PDF, EPUB, TXT, MD, HTML → Kindle-ready EPUB (cover, TOC, nav, clean metadata); runs on a worker thread |
| Send to Kindle | SMTP delivery to your `@kindle.com` address, plus a hand-off to `amazon.com/sendtokindle` |
| Focus mode | `⌘\` / `Ctrl+\` collapses both rails to a single page |
| Appearance | System, Light and Dark themes, persisted locally and switchable without reopening a book |
| Local-first storage | SQLite + files in place, no network required |

Deliberately deferred to 1.1: on-device transcription (whisper.cpp), OCR, automatic diagrams, and sync. The PRD covers those.

---

## Repository layout

```
stacks/
├── CMakeLists.txt
├── assets/
│   ├── fonts/            Newsreader, Inter, JetBrains Mono (OFL)
│   ├── textures/         Paper tooth and film grain, generated
│   └── samples/          Original sample PDFs and EPUBs
├── qml/                  QML shell, reader chrome, palette, diagram canvas
│   ├── Theme.qml         Reactive light/dark colours, faces and metrics
│   ├── ThemePicker.qml   System / Light / Dark appearance switcher
│   ├── Icon.qml          Vector icon set drawn on a canvas
│   ├── PillButton.qml    Primary / secondary / ghost buttons
│   ├── KeyCap.qml        Keyboard cap chip
│   ├── Cover.qml         Book jacket (cloth, spine, type)
│   ├── DeskView.qml      The home screen: headline, shelf, facts
│   ├── LibraryPane.qml   Shelf rail
│   ├── ReaderPane.qml    Page, folio, selection, composers
│   ├── MarginPane.qml    Annotation timeline
│   ├── CommandPalette.qml
│   ├── KindleSheet.qml    Convert a file and send it to Kindle
│   └── DiagramCanvas.qml
├── src/
│   ├── core/             Database, Library, PdfDocument, EpubDocument, AppController, VoiceRecorder
│   │                     KindleConverter (file → EPUB), KindleSender (SMTP to Kindle)
│   ├── native/           ReaderItem (QQuickPaintedItem: PDF + EPUB rendering and selection)
│   └── main.cpp
├── tests/                Qt Test end-to-end core and reader tests
├── third_party/pdfium/   Prebuilt PDFium (Linux x64 in this checkout)
└── tools/                Sample generator, e2e script
```

---

## Prerequisites

Ubuntu 24.04:

```bash
sudo apt-get update
sudo apt-get install -y \
  cmake ninja-build g++ \
  qt6-base-dev qt6-base-dev-tools \
  qt6-declarative-dev qt6-declarative-dev-tools \
  qt6-multimedia-dev qt6-svg-dev qt6-tools-dev qt6-l10n-tools \
  libsqlite3-dev libzip-dev zlib1g-dev libgl1-mesa-dev libxkbcommon-dev \
  qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts \
  qml6-module-qtquick-shapes qml6-module-qtquick-window qml6-module-qtquick-templates \
  qml6-module-qtquick-nativestyle qml6-module-qtqml-workerscript \
  qml6-module-qtmultimedia qml6-module-qtquick-dialogs qml6-module-qttest
```

PDFium: download the prebuilt binaries and unpack into `third_party/`:

```bash
mkdir -p third_party
curl -fsSL https://github.com/bblanchon/pdfium-binaries/releases/latest/download/pdfium-linux-x64.tgz \
  | tar -xz -C third_party
```

The checked-in binary is **Linux x64 only**. PDFium must match the OS and CPU
you are building for. CMake selects `.so`, `.dylib`, or `.dll`/`.lib` as appropriate;
use `-DPDFIUM_ROOT=/path/to/pdfium` for a separate distribution containing
`include/` and `lib/` (and `bin/` on Windows).

macOS (Apple Silicon):

```bash
brew install qtbase qtdeclarative qtmultimedia libzip ninja
mkdir -p third_party/pdfium-local
curl -fL https://github.com/bblanchon/pdfium-binaries/releases/download/chromium/8111/pdfium-mac-arm64.tgz \
  | tar -xz -C third_party/pdfium-local
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_PREFIX_PATH="$(brew --prefix)" -DPDFIUM_ROOT="$PWD/third_party/pdfium-local"
cmake --build build --parallel
```

For Intel Macs, use `pdfium-mac-x64.tgz` from the same release. CMake adjusts
the install name and ad-hoc signature of a **build-local copy** of the dylib;
the downloaded library is not modified. Windows uses `pdfium-win-x64.tgz`,
a Qt 6 SDK with Quick/Multimedia, and libzip/zlib (for example through vcpkg).
Pass the Qt prefix, vcpkg toolchain, and `PDFIUM_ROOT` to CMake; the PDFium DLL
is copied beside each executable. Qt's own runtime deployment remains required.

Fonts are already checked in under `assets/fonts/` (SIL Open Font License).

---

## Desktop downloads and CI

The [build and release workflow](.github/workflows/build-release.yml) produces
macOS Apple Silicon/Intel DMGs and ZIPs, Windows x64 installer/portable packages,
and Linux x64 AppImage/DEB/tar.gz packages. Every platform runs the core tests
and relocated-package smoke checks before publishing a version tag (`v0.1.0`,
for example). See [packaging/README.md](packaging/README.md) for release commands,
OS baselines, checksums and the unsigned/not-notarized signing limitations.

## Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
```

## Run

```bash
./build/stacks                 # launch the app
./build/stacks --import ~/book.epub
./build/stacks --open 3        # open a library id directly
./build/stacks --query margin  # headless full-text search
./build/stacks --data-dir ./scratch-library  # isolated library for testing
./build/stacks --theme dark    # system, light or dark; saved for next launch
```

On a headless machine:

```bash
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  ./build/stacks --screenshot /tmp/stacks.png
```

## Tests

```bash
ctest --test-dir build --output-on-failure
# or
QT_QPA_PLATFORM=offscreen STACKS_TEST_MODE=1 ./build/stacks_tests
```

The test suite covers:

- library seeding and metadata
- import and indexing for PDF and EPUB
- reading-position memory
- highlight, note, bookmark, and voice-note persistence
- FTS5 search ranking and snippets
- diagram nodes and edges
- PDF page rendering and text extraction
- EPUB HTML/text extraction
- `ReaderItem` load, page navigation, and locator resolution
- the Kindle converter: PDF → EPUB and EPUB → EPUB, checked by reading the
   produced file back with the app's own EPUB reader
- regression coverage for position flushes, EPUB restoration, preserved/generated
  navigation, concurrent PDFium access, conversion shutdown, annotation search
  migrations/edits, and the margin-delete/Kindle-selection UI paths
- a localhost SMTP server exercising verified STARTTLS and implicit TLS,
  certificate rejection, missing STARTTLS, timeouts, attachment rejection and retry
- appearance preference persistence, simulated OS theme notifications, live
  palette switching, primary-button contrast and EPUB recolouring without position loss
- resource and edge cases: voice recordings removed with their notes, diagram
  edges deduplicated, scoped per book and following a dragged node, bounded PDF
  renders, one page per trackpad swipe, EPUB progress reaching 100%, missing
  book files, HTML → EPUB, Kindle size limits and file names, overlay click
  handling, and pixel checks that rounded corners stay rounded

SMTP tests use an intentionally public localhost-only key/certificate under
`tests/fixtures/`. These are test fixtures, never application credentials.
Application-only builds can omit Qt Test with `-DBUILD_TESTING=OFF`.

## Full end-to-end run

```bash
tools/e2e.sh
```

This configures, builds, runs the Qt tests, runs the QML smoke test, renders welcome/EPUB/PDF screenshots, and verifies a headless search. It uses a fresh output/data directory and never deletes the shared `~/.qttest` directory. Extra CMake arguments can be passed to the script, for example `tools/e2e.sh -DPDFIUM_ROOT=/path/to/pdfium`.

---

## Appearance

Choose **System**, **Light**, or **Dark** in the bottom of the library sidebar.
System follows OS appearance changes automatically; explicit Light/Dark choices
override it. `⌘⇧T` / `Ctrl+Shift+T` cycles the three modes, including in focus
mode. The preference is saved in `<data dir>/appearance.ini`.

The palette covers the desk, rails, dialogs, diagrams, controls and EPUB text.
Changing it does not reload the book or move its reading position. PDFs retain
their original page colours. Reactive colour/type/metric tokens live in
`qml/Theme.qml`.

## Architecture notes

- **Core is pure C++ and has no UI dependency.** `Library` owns all SQL and indexing; `Database` owns the schema and FTS5 setup. This is the layer that later syncs, migrates, or gets reused by a CLI.
- **`ReaderItem` owns rendering and selection.** It is a `QQuickPaintedItem` so QML animates the shell (sidebar, palette, recorder, canvas) while C++ paints the reader itself. This is the split that keeps page turns and text layout native.
- **PDFium is reference-counted and process-wide serialized.** Every API call is protected by the same mutex, including worker-thread conversion, rendering and library teardown.
- **EPUB chapters are read through libzip, parsed to sanitized XHTML, and rendered as one continuous `QTextDocument`.** Highlights are stored as `epub:<start>-<end>` character ranges; search results use `epub:<chapter>:<paragraph>` and are resolved by finding the passage text.
- **Search is FTS5 with a `LIKE` fallback** for SQLite builds that lack FTS5. Query terms are quoted and prefix-matched, so partial words work.
- **Positions are debounced** in `AppController` (450 ms) so scrolling writes at most a few rows per second.
- **Navigation and shutdown flush pending positions.** Voice recordings are stopped and saved to the original book before leaving it; conversion workers are joined at shutdown.
- **Voice capture is intentionally simple.** `VoiceRecorder` streams 16 kHz mono WAV to disk via `QAudioSource`, so memory stays flat however long a note runs (recordings stop at two hours); deleting the note deletes its recording. Microphone access is checked first, since macOS otherwise records silence. Transcription is a 1.1 integration point for whisper.cpp or the platform speech API.
- **Sizes that come from files are bounded.** EPUB entries inflate to at most 64 MB each and 256 MB per book, PDF page bitmaps to 24 MP (8192 px a side), and Kindle attachments to Amazon's 50 MB and the mail server's advertised `SIZE`.
- **The home shelf is virtualised.** It is a `GridView`, so only the tiles in view exist and memory stays flat as the library grows.
- **Animation is transform/opacity only.** The rails, palette, and recorder use short eased transitions; the reader itself never animates layout.
- **The page is owned by QML, the content by C++.** `ReaderItem` paints text and PDF pages only; the paper, its tooth, the bound edge, the running head and the folio are QML layers around it, so the shell can be restyled without touching the renderer.

## Send to Kindle

Open the sheet from the reader's top bar (`⌘⇧K`), the home screen, or the
command palette.

1. **Pick a source** — any book in the library, or a file from disk
   (PDF, EPUB, TXT, MD, HTML, DOC, DOCX, RTF, images).
2. **Convert** — PDFs, text and HTML are re-flowed into a single Kindle-ready
   EPUB: a cover rendered from page one, one XHTML chapter per section, a
   navigation document, an NCX for older readers, and fixed Dublin Core
   metadata. An EPUB input is normalised instead of rebuilt: its own text and
   images, navigation, identifiers and package-relative paths are kept; a linked
   nav document is added only when one is missing. The result is written to
   `<data dir>/kindle/` without overwriting earlier conversions. HTML is reduced
   to its prose: headings start chapters; scripts, styles and tags are dropped.
3. **Send** — the EPUB is attached to an email addressed to your
   `@kindle.com` address and handed to Amazon's *Send to Kindle for Email*
   service, which accepts PDF, DOC, DOCX, TXT, RTF, HTM, HTML, images and
   EPUB. Amazon converts it to Kindle format on their side. You can also open
   `amazon.com/sendtokindle` and drop the file in yourself. Amazon accepts
   files up to 50 MB by email; larger ones are refused before anything uploads.

The sending account (from address, SMTP host, user, app password) is stored in
`<data dir>/kindle.json` with owner-only permissions on macOS/Linux. Windows
protects the password with per-user DPAPI encryption rather than relying on
POSIX mode bits; existing Windows plaintext files migrate on the next save.
The credentials never leave the machine except as part of the SMTP connection. An app password is the right credential
here — for Gmail, enable 2FA and create one under *App passwords*.

SMTP requires verified TLS: STARTTLS on the configured port, or implicit TLS
on port 465. The app refuses certificate errors and servers without STARTTLS,
and sends credentials only after encryption is established.

## Design

The visual language is documented in `design.md`, including the Mobbin
reference set that shaped it. Short version: a dark desk, a bound parchment
page for EPUBs, a loose white page for PDFs, one copper accent (with ochre,
moss and violet marking the other kinds of mark), Newsreader for books, Inter
for UI, JetBrains Mono for metadata, hairline borders, a margin that reads as
a timeline rather than a card stack, and a keyboard-first command palette.

Everything is defined in `qml/Theme.qml` and the shared components
(`Icon.qml`, `PillButton.qml`, `KeyCap.qml`, `Cover.qml`), so the shell has
one place to change.

## Performance targets

| Metric | Target |
|---|---|
| Cold launch | < 400 ms |
| Open book → first page | < 100 ms |
| PDF page turn | < 16 ms |
| Highlight response | < 16 ms |
| Search p95, 1M passages | < 50 ms |
| Baseline memory, 1,000 books | < 350 MB |

The prototype is structured to hit these on Linux/Windows/macOS hardware. The main risks to the budget are PDFium page caching, huge EPUB XHTML with complex CSS, and keeping the AI/voice work off the UI thread — all noted in the PRD.


### App identity and the reading desk

Stacks now has generated book-stack artwork in the sidebar, on the home screen,
and as its native application icon. On macOS, CMake creates `build/stacks.app`;
open it with `open build/stacks.app`. The `./build/stacks` command remains available
for CLI use. QML, fonts, textures and artwork are embedded by default; development builds
can still use checkout sample books and build-local dependencies. Use the
packaging workflow to bundle the runtime and create distributable downloads.
Configure `-DSTACKS_BUNDLED_RESOURCES=OFF` only for live QML editing.

The library workspace includes a continue-reading feature, live book counts,
sidebar navigation for All books / In progress / Finished, title-and-author
filtering, PDF/EPUB filters, recent/title/author sorting, and grid/list views.
Both presentations are virtualised: only visible books and a small scroll
buffer are instantiated. The filter controls stay in place while results change,
and shorter windows hide the reading feature to leave more room for books.

The reader has persistent previous/next buttons and a progress footer, a margin
with annotation counts and keyboard-accessible filters, and narrower side rails
on smaller windows. Action buttons support keyboard focus, Space/Enter, and
accessible names. Light, dark, and system appearance update the entire workspace
without reopening a book. Geometric fallback jackets give local books a
consistent visual identity.

Artwork provenance and the exact generation prompt are in
[`assets/branding/README.md`](assets/branding/README.md). New artwork gets a clean,
anti-aliased tile with `python3 tools/clean-icon-mask.py`; regenerate native icon
sizes with `bash tools/make-icons.sh` on macOS.
