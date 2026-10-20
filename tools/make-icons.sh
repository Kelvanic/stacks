#!/usr/bin/env bash
# Produce native macOS icon sizes from the original generated artwork.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ICONSET="$(mktemp -d "${TMPDIR:-/tmp}/stacks-icons.XXXXXX")/Stacks.iconset"
mkdir -p "$ICONSET"
for size in 16 32 128 256 512; do
    sips -z "$size" "$size" "$ROOT/assets/branding/stacks-icon.png" --out "$ICONSET/icon_${size}x${size}.png" >/dev/null
    double=$((size * 2))
    sips -z "$double" "$double" "$ROOT/assets/branding/stacks-icon.png" --out "$ICONSET/icon_${size}x${size}@2x.png" >/dev/null
done
iconutil -c icns "$ICONSET" -o "$ROOT/assets/branding/Stacks.icns"
rm -r "$(dirname "$ICONSET")"
