#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BUILD_DIR="${BUILD_DIR:-build}"
OUT_DIR="${OUT_DIR:-$(mktemp -d "${TMPDIR:-/tmp}/stacks-e2e.XXXXXX")}"
mkdir -p "$OUT_DIR"

step() { printf '\n==> %s\n' "$1"; }
run_app() {
    QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software STACKS_TEST_MODE=1 \
        "./$BUILD_DIR/stacks" --data-dir "$OUT_DIR/library" "$@"
}

step "Configure"
cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo "$@"

step "Build"
cmake --build "$BUILD_DIR" --parallel

step "Unit + integration tests"
ctest --test-dir "$BUILD_DIR" --output-on-failure

step "QML smoke test"
run_app --smoke-test 2>&1 | tee "$OUT_DIR/smoke.log"

step "Render welcome screen"
run_app --screenshot "$OUT_DIR/welcome.png"
test -s "$OUT_DIR/welcome.png"

step "Render EPUB reader"
run_app --open 2 --screenshot "$OUT_DIR/reader-ebook.png"
test -s "$OUT_DIR/reader-ebook.png"

step "Render EPUB reader with marks"
run_app --open 2 --demo --screenshot "$OUT_DIR/reader-marks.png"
test -s "$OUT_DIR/reader-marks.png"

step "Render diagram canvas"
run_app --open 2 --demo --diagram --screenshot "$OUT_DIR/diagram.png"
test -s "$OUT_DIR/diagram.png"

step "Render PDF reader"
run_app --open 3 --screenshot "$OUT_DIR/reader-pdf.png"
test -s "$OUT_DIR/reader-pdf.png"

step "Headless search"
run_app --query "margin" > "$OUT_DIR/search.txt"
grep -q "Systems That Breathe" "$OUT_DIR/search.txt"
grep -q "The Art of Attention" "$OUT_DIR/search.txt"

step "Result"
printf 'ok\nscreenshots: %s\n' "$OUT_DIR"
