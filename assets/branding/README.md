# Stacks artwork

`stacks-icon.png` is the generated artwork with a transparent margin. Its
alpha channel was rebuilt with `python3 tools/clean-icon-mask.py` — a fully
opaque tile with an anti-aliased rounded edge in place of the generator's
stepped, slightly translucent cut. Run that on any new artwork first.
`Stacks.icns` contains the native macOS size variants, regenerated with
`bash tools/make-icons.sh` on macOS. The PNG is embedded in the executable for
the application/window icon and is also used by `qml/BrandMark.qml`.

Created with the built-in imagegen tool. Final generation prompt:

```
Use case: logo-brand
Asset type: production macOS application icon for Stacks, a calm personal reading desk.
Primary request: a beautifully crafted, distinctive app icon featuring three small stacked hardcover books, the uppermost book slightly open with cream paper pages and a narrow copper bookmark. The silhouette should be simple and instantly legible at small sizes.
Style/medium: premium tactile 3D product illustration, restrained craftsmanship, softly beveled clothbound covers, subtle paper edges, precise geometry. A deep warm espresso rounded-square tile with generous rounded corners, copper terracotta and muted sage book covers, warm ivory page edges. Books seen at a gentle three-quarter angle, centered and filling about 70 percent of the tile. Soft studio lighting from upper left, subtle realistic contact shadows, no dramatic glow. The rounded-square tile itself occupies 88 percent of the square canvas with equal transparent margins.
Constraints: one icon only, square composition, actual transparent background outside the rounded-square tile, no lettering, no text, no border, no surrounding objects, no watermark. Polished and calm, designed as an actual desktop app icon.
```

In-app action icons are code-drawn in `qml/Icon.qml` for crisp sizing and live
theme colors. Book jackets use deterministic geometric fallback artwork in
`qml/Cover.qml`; these are not publisher cover images.
