# Stacks — Design Direction

| Field | Value |
|---|---|
| Status | Shipped — v2 interface |
| Date | 2026-10-20 |
| Design thesis | A quiet instrument, not a dashboard |
| References | Mobbin MCP (connected): Kindle web reader, Matter, Literal, Craft, Bear, Spotify, ElevenLabs, Gumloop, Vapi, Juicebox, ElevenLabs manuscript, NYT conversation panel |

---

## 1. The feeling

Stacks should feel like a **well-made reading instrument**: warm paper, precise typography, restrained color, and almost no chrome.

Not:

- a productivity dashboard
- a glassmorphism demo
- an AI product with sparkles everywhere
- a library management tool with 14 toolbars

Three adjectives: **calm, tactile, precise.**

The reader is the hero. Everything else recedes.

---

## 2. Visual concept

**“Warm ink on paper, with copper for the things you add.”**

- The book page stays paper-warm, even in dark mode.
- The UI shell is ink-dark, almost black, with hairline borders.
- A single copper accent marks highlights, voice recording, and active states.
- Notes and diagrams feel like index cards pressed onto the page — not another app inside the app.

Avoid:

- blue-purple SaaS gradients
- neon AI glow
- frosted glass on every panel
- rounded cards inside rounded cards
- drop shadows that look like 2012

---

## 3. Design tokens

### 3.1 Color — dark (default)

Live values live in `qml/Theme.js`; this table is the contract.

| Token | Value | Use |
|---|---|---|
| `--ink-0` | `#0A0908` | Desk behind the page |
| `--ink-1` | `#12100E` | Rails (library, margin, top bar) |
| `--ink-2` | `#1A1714` | Cards, raised surfaces |
| `--ink-3` | `#241F1A` | Hover / selected |
| `--ink-4` | `#312A22` | Pressed, delete affordance |
| `--paper` | `#F6EFE0` | Bound page (EPUB) |
| `--paper-ink` | `#211C16` | Body text on paper |
| `--paper-muted` | `#6E6558` | Meta text on paper |
| `--text` | `#F0E9DB` | Primary UI text |
| `--text-muted` | `#A2988A` | Secondary text |
| `--text-faint` | `#6F675B` | Tertiary text |
| `--text-ghost` | `#4A443C` | Section labels |
| `--copper` | `#C9703F` | Accent, highlights, record |
| `--copper-bright` | `#E08B57` | Hover on copper, match text |
| `--copper-soft` | `rgba(201,112,63,0.20)` | Active chip wash |
| `--moss` | `#8FA07A` | Bookmarks |
| `--ochre` | `#D3A05C` | Notes |
| `--violet` | `#8E7CB4` | Voice notes |
| `--rose` | `#C2706A` | Destructive |
| `--line` | `rgba(240,233,219,0.08)` | Hairline borders |
| `--line-strong` | `rgba(240,233,219,0.18)` | Focus borders |

Four accent hues carry meaning, not decoration: copper = highlight, ochre = note,
moss = bookmark, violet = voice. Every mark shows its kind in text as well, so
colour is never the only signal.

### 3.2 Color — light

| Token | Value | Use |
|---|---|---|
| `--ink-0` | `#F4EFE5` | App background |
| `--ink-1` | `#EDE6D8` | Sidebar / panel |
| `--ink-2` | `#E5DCCB` | Raised surface |
| `--paper` | `#FFFDF7` | Reader page |
| `--text` | `#201C16` | Primary text |
| `--text-muted` | `#6F665A` | Secondary text |
| `--copper` | `#A9502A` | Accent |
| `--line` | `rgba(32,28,22,0.12)` | Hairline |

### 3.3 Typography

| Role | Face | Size / weight |
|---|---|---|
| Display (desk headline, cover) | **Newsreader** (OFL) | 56 px / 25–30 px, 500 |
| Book title (hero, top bar) | **Newsreader** | 19 px, 500 |
| Quote in the margin | **Newsreader** | 15 px, italic |
| UI / body | **Inter** (OFL) | 12–14 px, 400–500 |
| UI labels | Inter | 11–12 px, 500 |
| Reader body (EPUB) | **Newsreader** | 17 pt, 1.62 line-height |
| Metadata / shortcuts | **JetBrains Mono** (OFL) | 7.5–10 px, +0.1–1.9em tracking |

Rules:

- No more than two typefaces on screen at once.
- Reader line length: 58–72 characters.
- UI text never below 12 px.
- Use tabular numerals for page numbers and progress.

### 3.4 Spacing

8-point grid. Use `4, 8, 12, 16, 24, 32, 48, 64`.

- Panel padding: 20 px.
- Reader side margin: 64 px minimum.
- Card gap: 12 px.
- Section gap: 32 px.

### 3.5 Radii and elevation

- Radius: `14px` for hero cards and the palette, `12px` for sheets, `10px` for
  rows, `9px` for buttons and inputs, `3px` for the page itself.
- Borders: 1 px hairline. Prefer borders over shadows.
- Shadow is reserved for things that float above the page: the palette, the
  note composer, the recorder, the page on the desk.
- Paper is never flat: `assets/textures/paper.png` tiles a tooth over the
  page, and the whole window carries `assets/textures/grain.png` at 45%.

### 3.6 Motion

- Standard: `160ms cubic-bezier(0.2, 0.8, 0.2, 1)`.
- Quick: `100ms`.
- Page transition: crossfade + 4 px lift, 140 ms.
- Command palette: 120 ms opacity + 8 px scale from 0.98.
- Voice waveform: live amplitude, no fake animation.
- Respect Reduce Motion.

---

## 4. Layout

### 4.1 Main window

```
┌────────────┬────────────────────────────────────────┬────────────┐
│  Stacks    │  Systems That Breathe      ⌕ ▰ ◇ ✎ ◉ ⌗ ▣ │  MARGIN  ③ │
│  ⌕ search  │  MIRA CHEN · EPUB · 29% READ             │  ALL MARKS │
│            │ ─────────────────────────────────────── │  NOTES     │
│ NOW READING│                                        │  VOICE     │
│ ┌────────┐ │            ┌──────────────┐            │  ●────────  │
│ │cover   │ │  ‹        │  bound page  │        ›   │  │ quote     │
│ │title   │ │            │              │            │  │ note      │
│ │▓▓░ 29% │ │            │  (loose PDF  │            │  ○────────  │
│ └────────┘ │            │   page, too) │            │  │ quote     │
│            │            └──────────────┘            │            │
│ SHELF      │   29% READ  ──────▓▓░░░  PAGE 3 OF 12 │  Diagram   │
│ ▤ book     │                                        │  canvas    │
│ ▤ book     │                                        │            │
└────────────┴────────────────────────────────────────┴────────────┘
```

- Left rail 320 px, right margin 348 px, both animated.
- **Focus mode** (`⌘\`) collapses both rails and centres the page.
- The top bar is not a toolbar. It carries the title, the meta line, a
  progress hairline, and icon buttons whose labels rise on hover.
- The page: EPUBs are a bound parchment sheet with a spine shadow; PDFs are a
  loose white page lying on the dark desk, as in the Kindle web reader.
- Reader max width: 720 px for text; PDF fits the available area.
- A running head fades in over the bound page once you are into the book.
- No visible scrollbars unless scrolling.

### 4.2 Library

- “Now Reading” is one hero card: jacket, title in Newsreader, author in
  mono caps, a copper progress rule, and a “Continue reading” action.
- The shelf is a dense list, not a grid. Cover 36×50, title, author, format,
  and a progress hairline; the open book carries a copper spine.
- Below 64 px wide, a jacket shows cloth only — no text, no stamp.
- Collections are folders with a count, not coloured chips.
- Empty state: “Drop a PDF or EPUB anywhere.”

### 4.3 Reader

- The bound page: parchment, 1 px warm edge, an 18 px bound edge on the left,
  paper tooth over the whole sheet.
- Folio below the page: percent on the left, a 260 px progress rule in the
  middle, page hint on the right.
- Highlights: copper fill, no rounded marker.
- The margin rail is a **timeline**, not a card stack: a hairline thread with a
  coloured dot per mark, the quote in Newsreader italic, the note in Inter, and
  a mono meta line (`HIGHLIGHT · PAGE 3 · OCT 20`).
- Filters across the top of the margin: ALL / MARKS / NOTES / VOICE.
- Voice notes: thin violet waveform + duration.
- Diagram chip: a drawn mini-map + “Turn the margin into a map”.

### 4.4 Command palette

- 740 px wide, 86 px from the top, height follows the result count.
- Search field at top, no border, 18 px type, magnifier drawn as an icon.
- Each result: a PDF/EPUB badge, the book title, the locator in mono, and a
  snippet with the match in copper bold.
- Selected row: `--ink-3` with a 2 px copper left rule.
- Empty query shows a serif line — “Search the whole desk.” — plus the
  keyboard map, so the overlay teaches instead of sitting empty.
- Footer: `↑↓ navigate · ↵ open · esc close` and a result count.

### 4.5 Diagram canvas

- Dark ink sheet with a faint dot grid.
- Nodes are parchment cards, 188 px wide, with a copper left rule and a
  `MARGIN NOTE` chip at the bottom.
- Edges are 1.6 px copper curves drawn under the cards.
- Selected node: copper border, no glow.
- Header carries Connect / Add node / close, matching the reader chrome.

### 4.6 Voice recorder

- Small floating pill above the page, 360 px wide.
- Live waveform in copper, timer in mono.
- Buttons: cancel, stop, pause.
- After stop: transcript card with editable text and “Attach to highlight.”
- No modal takeover. The page stays visible.

---

### 4.7 Send to Kindle sheet

- A full overlay, 960 px wide, that answers two questions: *what am I sending*
  and *where is it going*.
- Left: the library as sources, each with its jacket and format, and an
  “Add a file…” escape hatch for anything on disk.
- Right: the source card (what the conversion will do), the result card
  (file name, pages, words, size), the actions, the destination address, and
  the sending account.
- The conversion result is the loudest thing in the right column: a copper
  “READY TO SEND” label, then the file name in the UI face.
- The destination field is a plain input, with a sentence under it that says
  where Amazon keeps the address. No wizard, no account linking.
- Sending credentials are collapsed behind “SENDING ACCOUNT / EDIT” and
  described as kept on this machine only.
- The footer carries the live status line, so a conversion or a failed SMTP
  handshake is reported where the eye already is.

## 5. Component specs

### Book row

- Height 70 px.
- Cover 36×50 (cloth only — text is hidden below 64 px).
- Title 13.5 px/500; author 12 px/muted; format in mono caps.
- Progress: 2 px hairline at the bottom, copper fill.
- Selected: copper spine on the left, `--ink-2` background.
- Hover: chevron appears on the right.

### Margin entry

- No card. A dot on a hairline thread, then content.
- Quote in Newsreader 15 px italic (4 lines max).
- Note in Inter 12 px (5 lines max).
- Meta in mono 7.5 px: kind · page · date.
- Hover reveals a close button.

### Search result

- 78 px row.
- PDF/EPUB badge 34×34.
- Title 12.5 px/500 + locator in mono 8 px.
- Snippet 11.5 px, matched terms in copper bold.

### Diagram node

- Parchment card, 9 px radius, 188×100.
- Label in Newsreader 15 px/500.
- Body 11 px, 2 lines max.
- `MARGIN NOTE` chip in mono 8 px, copper.
- Drag handle: entire card; no visible handle.

### Buttons

- Primary: copper fill, ink text, 8 px radius, 34 px height.
- Secondary: `--ink-2` fill, 1 px line.
- Icon button: 34×34, label rises on hover as a chip.
- Keycaps: mono 9 px on a 5 px radius plate.
- No pill buttons except filters.

---

## 6. Iconography

- 1.6 px stroke, rounded caps and joins, 18×18 grid.
- Drawn on a `Canvas` in `qml/Icon.qml` — never a font glyph, so the geometry
  is identical on every platform.
- Set: search, highlight, bookmark, note, mic, diagram, plus, close,
  chevrons, arrow-right, trash, clock, spark, quote, panel-left,
  panel-right, grid, list, book, layers.
- No filled icons except active states.
- No emoji in UI.

---

## 7. Copy and tone

- Short, calm, literal.
- “Continue reading” not “Jump back in.”
- “Voice note” not “Capture your thoughts with AI.”
- “Search everything” not “Supercharge your knowledge.”
- Errors explain the fix: “This PDF is password-protected. Unlock it in Preview, then import again.”
- AI copy always says what it used: “Generated from 4 passages in this book.”

---

## 8. Accessibility

- Contrast: body text ≥ 7:1; muted text ≥ 4.5:1.
- Focus ring: 2 px copper, 2 px offset.
- All actions keyboard-reachable.
- VoiceOver labels for every icon button.
- Reduced motion respected.
- Reader font size and line height adjustable.
- No color-only meaning: highlight colors also have labels.

---

## 9. Mobbin reference set (pulled 2026-10-20)

Mobbin MCP is connected. These screens were pulled and each one changed
something concrete in the interface:

| Reference | What it gave us |
|---|---|
| Amazon Kindle (web reader) | A warm page on a dark surround; a minimal top bar; the reader page as the only bright object on screen → PDFs became loose pages on the desk |
| Google Gemini “book” spread | Paper that reads as a bound object; a spine edge rather than a flat rectangle |
| Matter (article + notebook rail) | The right-hand rail as a working margin with the quote as the hero, and a tool rail that stays out of the way |
| Literal (library) | Cover thumbnails + sparse metadata; a shelf that is a list, not a grid |
| Bear | Warm cream surfaces, one confident accent, an editor that is mostly type |
| Craft | A left index, a right inspector, and a canvas that keeps its own visual language |
| ElevenLabs manuscript | Chapters in a rail, manuscript centred, transport controls in the chrome |
| Spotify (library) | Left rail + hero + horizontal shelf, with a single strong accent |
| Gumloop / Vapi / Juicebox (palettes) | Compact palettes, grouped results, keyboard hints in the footer, match highlighting inside snippets |
| The New York Times conversation panel | A rail that carries a count and filters, not a stack of cards |

Deliberately **not** copied: the card-stacked margin, glass panels, gradient
headers, and multi-coloured highlight systems.

Two rules held throughout: one accent colour carries state, and every screen
keeps the reader as the brightest thing on the display.

---

## 10. What “sexy” means here

Sexy is not more effects. It is:

- one strong typeface choice
- one accent color
- generous negative space
- precise alignment
- quiet motion
- a reader that feels like paper
- a command palette that feels like a secret power
- no visible complexity until asked

The app should look expensive because it is restrained, not because it is decorated.
