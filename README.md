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
│   ├── Theme.js          Every colour, face and metric
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

SMTP tests use an intentionally public localhost-only key/certificate under
`tests/fixtures/`. These are test fixtures, never application credentials.
Application-only builds can omit Qt Test with `-DBUILD_TESTING=OFF`.

## Full end-to-end run

```bash
tools/e2e.sh
```

This configures, builds, runs the Qt tests, runs the QML smoke test, renders welcome/EPUB/PDF screenshots, and verifies a headless search. It uses a fresh output/data directory and never deletes the shared `~/.qttest` directory. Extra CMake arguments can be passed to the script, for example `tools/e2e.sh -DPDFIUM_ROOT=/path/to/pdfium`.

---

## Architecture notes

- **Core is pure C++ and has no UI dependency.** `Library` owns all SQL and indexing; `Database` owns the schema and FTS5 setup. This is the layer that later syncs, migrates, or gets reused by a CLI.
- **`ReaderItem` owns rendering and selection.** It is a `QQuickPaintedItem` so QML animates the shell (sidebar, palette, recorder, canvas) while C++ paints the reader itself. This is the split that keeps page turns and text layout native.
- **PDFium is reference-counted and process-wide serialized.** Every API call is protected by the same mutex, including worker-thread conversion, rendering and library teardown.
- **EPUB chapters are read through libzip, parsed to sanitized XHTML, and rendered as one continuous `QTextDocument`.** Highlights are stored as `epub:<start>-<end>` character ranges; search results use `epub:<chapter>:<paragraph>` and are resolved by finding the passage text.
- **Search is FTS5 with a `LIKE` fallback** for SQLite builds that lack FTS5. Query terms are quoted and prefix-matched, so partial words work.
- **Positions are debounced** in `AppController` (450 ms) so scrolling writes at most a few rows per second.
- **Navigation and shutdown flush pending positions.** Voice recordings are stopped and saved to the original book before leaving it; conversion workers are joined at shutdown.
- **Voice capture is intentionally simple.** `VoiceRecorder` writes 16 kHz mono WAV via `QAudioSource`; transcription is a 1.1 integration point for whisper.cpp or the platform speech API.
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
   `<data dir>/kindle/` without overwriting earlier conversions.
3. **Send** — the EPUB is attached to an email addressed to your
   `@kindle.com` address and handed to Amazon's *Send to Kindle for Email*
   service, which accepts PDF, DOC, DOCX, TXT, RTF, HTM, HTML, images and
   EPUB. Amazon converts it to Kindle format on their side. You can also open
   `amazon.com/sendtokindle` and drop the file in yourself.

The sending account (from address, SMTP host, user, app password) is stored in
`<data dir>/kindle.json` with owner-only permissions and never leaves the
machine except as the mail it sends. An app password is the right credential
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

Everything is defined in `qml/Theme.js` and the shared components
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
