# Stacks — Product Requirements Document

| Field | Value |
|---|---|
| Product | Stacks |
| Document | Product Requirements Document (PRD) |
| Version | 0.1 — draft for review |
| Date | 2026-10-19 |
| Owner | Product / Founder |
| Status | Draft |
| Target | macOS 1.0 |
| Tagline | **Read it. Say it. Map it. Find it.** |

---

## 1. Executive summary

Stacks is a local-first macOS reading desk for people with large PDF and EPUB libraries. It combines a native reader, a library manager, highlights and bookmarks, voice notes, and a diagram canvas in one app — then makes all of it searchable from a single command palette.

The core promise:

> **Every PDF and EPUB you own, instantly searchable — with your place, highlights, bookmarks, voice notes, and diagrams remembered.**

Stacks is not a general note-taking app and not an ebook store. Every note, voice clip, and diagram is anchored to a book, page, or passage. That constraint is the product.

The wedge is search. Existing tools either manage files (Calibre), annotate one document (Preview, PDF Expert), capture highlights (Readwise), or store notes (Obsidian). Stacks connects reading, recall, and thinking in one local index.

The first release targets researchers, graduate students, lawyers, consultants, and other heavy nonfiction readers — people who already have hundreds of PDFs and a messy folder hierarchy.

---

## 2. Problem

### 2.1 The user’s current workflow is fragmented

A typical heavy reader:

1. Stores PDFs and EPUBs across Finder folders, Downloads, email attachments, Zotero, and cloud drives.
2. Reads in Preview, Apple Books, Kindle, or a browser.
3. Loses their place when switching devices or apps.
4. Highlights in one app, writes notes in another, and records voice memos in a third.
5. Cannot search across all of it. Search means remembering which app, folder, or document contains the passage.
6. Re-reads or re-finds the same material repeatedly.

### 2.2 Existing tools solve only one slice

- **Calibre** is powerful but has a dated, complex interface and is primarily a library/conversion tool, not a modern reading and thinking environment.
- **Apple Books** is a store and a casual reader; it is weak for large PDF libraries, annotations, and cross-format search.
- **PDF Expert / Preview** annotate one file at a time; they do not provide a library-wide index or linked notes.
- **Readwise** captures highlights but is not a reader or a local library.
- **Obsidian / Notion** store notes but are disconnected from the source passage unless the user manually links everything.
- **DEVONthink** is powerful but heavy, expensive, and not designed around a calm reading experience.
- **MarginNote / LiquidText** are strong for deep study but are not lightweight, fast, local-first library tools for everyday reading.

### 2.3 The gap

There is no fast, native macOS app that treats **reading, recall, voice capture, visual thinking, and search** as one workflow — while keeping the library local and the interaction keyboard-first.

---

## 3. Vision and principles

### 3.1 Vision

A reader opens Stacks, continues exactly where they left off, highlights a passage, speaks a thought, turns that thought into a diagram, and later finds all three with one search — without thinking about files, folders, or syncing.

### 3.2 Product principles

1. **Local-first by default.** Book content, notes, voice recordings, and embeddings stay on the user’s Mac unless the user explicitly enables sync.
2. **Anchored, not floating.** Every note and diagram has a source link: book, page, CFI, highlight, or timestamp.
3. **Keyboard-first, mouse-friendly.** The core loop should work without leaving the keyboard.
4. **Fast enough to feel invisible.** Search results in under 150 ms for a typical library; page turns and highlights should feel instant.
5. **Readable, not clever.** The UI should disappear behind the text. No dashboard clutter, no gamification, no AI noise.
6. **No DRM circumvention.** Stacks imports files the user owns and can open. It does not remove DRM or redistribute content.
7. **One-time purchase, no AI tax.** Local models mean no per-query server cost. A one-time license is viable.
8. **Graceful degradation.** Core reading and search work on macOS 14+. AI features require newer hardware/OS and are clearly labeled.

---

## 4. Target users

### 4.1 Primary persona — The researcher

**Maya, 29, PhD candidate in public policy.**

- 800+ PDFs, 150 EPUBs, hundreds of highlights.
- Reads across laptop and iPad; loses her place constantly.
- Uses Zotero for citations but Preview for reading.
- Wants to search “the passage about regulatory capture in the telecom case” without remembering the file.
- Would pay for a fast, private tool that keeps her library on her Mac.

**Jobs to be done:**
- “When I’m reading a paper, I want to capture a thought without losing my place.”
- “When I need a quote, I want to find it across all my books in seconds.”
- “When I’m synthesizing sources, I want to turn notes into a visual map.”

### 4.2 Secondary persona — The knowledge professional

**Daniel, 41, consultant / lawyer / analyst.**

- 1,000+ client PDFs, reports, contracts, and books.
- Needs to find exact clauses, figures, and prior notes quickly.
- Cannot send confidential documents to cloud AI tools.
- Values local storage, keyboard shortcuts, and export to Markdown.

### 4.3 Tertiary persona — The technical reader

**Priya, 34, staff engineer.**

- Reads RFCs, papers, internal docs, and technical books.
- Wants code snippets, diagrams, and highlights searchable.
- Willing to pay for a native app that respects privacy and works offline.

### 4.4 Anti-personas

- Casual readers who only buy books from Apple Books or Kindle.
- Users who want a social reading network or book club.
- Users who need a full reference manager with citation styles (Zotero remains the source of truth).
- Users who expect DRM-protected Kindle/Apple Books files to open.

---

## 5. Jobs to be done

| ID | Job | Desired outcome |
|---|---|---|
| JTBD-1 | Continue reading | Open the app and resume the exact page/position without searching. |
| JTBD-2 | Capture a thought | Highlight a passage and attach a note in under 5 seconds. |
| JTBD-3 | Capture a spoken thought | Speak a note and get an accurate, searchable transcript attached to the passage. |
| JTBD-4 | Find a passage | Search all books, highlights, notes, and transcripts and jump to the exact source. |
| JTBD-5 | Synthesize | Turn a set of notes into a diagram with citations back to the source. |
| JTBD-6 | Export | Send highlights and notes to Markdown, Obsidian, or Readwise. |
| JTBD-7 | Trust the tool | Know that book content never leaves the Mac unless sync is explicitly enabled. |

---

## 6. Competitive landscape and differentiation

| Product | Strength | Gap Stacks exploits |
|---|---|---|
| Calibre | Library management, conversion, device transfer | Dated UI, weak reading/annotation, no modern search/voice/diagrams |
| Apple Books | Native, simple, store integration | Weak PDF library management, limited annotations, no cross-library search |
| Kindle | Ecosystem, sync | Closed format, weak PDF experience, not local-first |
| PDF Expert | Polished PDF editing/annotation | One file at a time, no library-wide index, no notes/voice workflow |
| Preview | Free, ubiquitous | Minimal library and annotation features, no search across documents |
| Readwise | Highlight capture and review | Not a reader, not a local library, subscription |
| Obsidian | Flexible notes and linking | Not a reader, manual source linking, plugin complexity |
| DEVONthink | Powerful indexing and search | Heavy, expensive, steep learning curve, not reading-first |
| MarginNote / LiquidText | Deep study and mind maps | Complex, iPad-first, not a fast everyday Mac library |
| Zotero | Reference management | Not a reading/annotation environment, weak PDF search UX |

### 6.1 Differentiation

1. **One index for books, highlights, notes, voice transcripts, and diagram labels.**
2. **Voice notes captured in context** — not a separate recorder app.
3. **Diagrams generated from a note or passage**, with citations back to the source.
4. **Native macOS performance and keyboard-first UX.**
5. **Local-first privacy** with no mandatory cloud account.
6. **One-time purchase** rather than a subscription.

### 6.2 Positioning statement

For researchers and knowledge workers with large PDF/EPUB libraries who are tired of juggling Preview, Calibre, notes apps, and voice memos, **Stacks** is a local-first reading desk that remembers your place, captures your thoughts in text or voice, turns them into diagrams, and makes everything instantly searchable. Unlike Calibre or Readwise, Stacks treats reading, recall, and synthesis as one workflow — and never sends your books to a server.

---

## 7. Scope

### 7.1 MVP (1.0)

- macOS 14+ (Sonoma or later), Apple silicon and Intel.
- Import PDF and EPUB files.
- Library with metadata, covers, collections, tags, smart collections.
- PDF reader with page navigation, zoom, search, position memory.
- EPUB reader with chapter navigation, font controls, position memory.
- Highlights, bookmarks, and text notes.
- Voice notes with on-device transcription, attached to a page/highlight/book.
- Global search across book text, highlights, notes, and transcripts.
- Basic diagram canvas: create a diagram, add nodes/edges, link nodes to sources.
- Export highlights/notes to Markdown; export diagrams to PNG/SVG.
- Local-only storage; optional iCloud sync is **not** in 1.0.
- No account required.

### 7.2 1.1

- Auto-diagram from a selected note/passage using an on-device model.
- OCR for scanned PDFs (Vision).
- Kindle/Kobo handoff, including the file-to-Kindle converter (built early in the prototype).
- Readwise/Obsidian export.
- Optional iCloud sync (metadata and annotations, not book files).
- “Ask your library” with citations (beta).

### 7.3 1.2+

- Semantic/hybrid search with on-device embeddings.
- iPad companion (read-only or full sync).
- Advanced citation styles and Zotero bridge.
- Shared libraries / collaboration (only if validated).
- Windows/Linux (not planned for 1.x).

### 7.4 Non-goals

- DRM removal or conversion of protected Kindle/Apple Books files.
- A full ebook store or storefront.
- A general-purpose note-taking app (no daily journal, task manager, or wiki).
- A full vector illustration or whiteboard tool.
- Social features, book clubs, or public sharing.
- Cloud AI processing of book content by default.
- Citation management replacement for Zotero.

---

## 8. User experience

### 8.1 Core loop

1. **Open** Stacks → “Continue reading” shows the last 3 books with progress.
2. **Read** → PDF/EPUB opens at the remembered position.
3. **Capture** → select text → highlight → `⌘⇧V` to speak a note (or `⌘⇧N` for text).
4. **Map** → `⌘⇧D` turns the note or selection into a diagram.
5. **Find** → `⌘K` searches everything; results show book, page/CFI, snippet, and type.
6. **Export** → send highlights/notes to Markdown or copy a citation.

### 8.2 Information architecture

```
Stacks
├── Library
│   ├── All Books
│   ├── Continue Reading
│   ├── Collections
│   ├── Tags
│   ├── Smart Collections
│   └── Recently Added
├── Reader
│   ├── PDF view
│   ├── EPUB view
│   ├── Outline / thumbnails
│   ├── Search in book
│   └── Annotations panel
├── Notes
│   ├── All notes
│   ├── By book
│   ├── Voice notes
│   └── Unlinked notes (inbox)
├── Diagrams
│   ├── All diagrams
│   ├── By book
│   └── Canvas
├── Search
│   ├── Command palette (⌘K)
│   └── Advanced search
└── Settings
    ├── Library
    ├── Reading
    ├── Voice & AI
    ├── Privacy
    ├── Sync (1.1)
    └── About
```

### 8.3 Key flow — capture a spoken note

1. User selects a passage in the reader.
2. Presses `⌘⇧V`.
3. A small recorder appears over the page; waveform + timer.
4. User speaks; presses `Return` or clicks Stop.
5. On-device transcription runs; transcript appears in an editable popover.
6. User can edit, tag, or save.
7. The note is stored with:
   - book ID
   - page/CFI
   - highlight ID (if any)
   - transcript
   - optional audio file
   - timestamp
8. The annotation appears in the margin and in the Notes panel.
9. A toast offers “Turn into diagram” and “Copy citation.”

**Edge cases:**
- No microphone permission → explain and link to System Settings.
- On-device model unavailable → offer text note or download a local model.
- Long recording → transcribe in chunks; show progress; allow cancel.
- No speech detected → keep audio, show “No transcript” and let user type.

### 8.4 Key flow — search everything

1. Press `⌘K`.
2. Type a query.
3. Results are grouped: Books, Passages, Highlights, Notes, Voice, Diagrams.
4. Each result shows a snippet with matched terms, source, and date.
5. Arrow keys move; `Return` opens the source at the exact location.
6. `⌘↵` opens in a new window; `⌥↵` copies a citation.
7. Filters: type, book, tag, date, “has notes,” “has voice,” “has diagram.”
8. Empty state suggests recent searches and “search inside scanned PDFs” if OCR is available.

### 8.5 Key flow — diagram from a note

1. User opens a note or selects a passage.
2. Presses `⌘⇧D` → “Create diagram.”
3. On-device model extracts entities and relationships; user sees a preview.
4. User confirms; a canvas opens with nodes and edges.
5. Each node has a citation chip linking back to the book/page.
6. User can rename, recolor, add nodes, or drag.
7. Export as PNG/SVG/PDF or embed in a note.

**Guardrail:** auto-generated diagrams must be editable and must show citations. The model never invents sources; if it cannot ground a node, the node is marked “unsourced.”

---

## 9. Functional requirements

Priority: **P0** = required for 1.0; **P1** = 1.1; **P2** = later.

### 9.1 Library and import

| ID | Requirement | Priority | Acceptance criteria |
|---|---|---|---|
| FR-LIB-01 | Import PDF and EPUB files via file picker and drag-and-drop. | P0 | Files appear in library within 2 seconds per 100 files; unsupported files are skipped with a clear message. |
| FR-LIB-02 | Watch folders for new files. | P0 | User can add up to 10 watched folders; new files are indexed automatically; opt-out per folder. |
| FR-LIB-03 | Extract metadata: title, author, publisher, year, language, cover. | P0 | Metadata is fetched from embedded metadata first, then Open Library/Google Books if online and enabled. |
| FR-LIB-04 | Detect duplicates by file hash and title/author similarity. | P0 | Duplicate candidates are shown before import; user can merge or keep both. |
| FR-LIB-05 | Organize with collections, tags, and smart collections. | P0 | Smart collection rules support format, tag, author, year, read state, and annotation count. |
| FR-LIB-06 | Preserve original files; no proprietary library format required. | P0 | Deleting Stacks does not delete the user’s books. Optional “managed library” copy mode is explicit. |
| FR-LIB-07 | Handle missing/moved files. | P0 | Book shows “File missing”; user can relink; metadata and annotations are preserved. |
| FR-LIB-08 | Import MOBI/AZW3. | P2 | Only if legally permissible and conversion is not required. |
| FR-LIB-09 | Calibre library import. | P1 | Import metadata and file paths from a Calibre library without modifying it. |

### 9.2 Reader

| ID | Requirement | Priority | Acceptance criteria |
|---|---|---|---|
| FR-READ-01 | Render PDFs with PDFKit. | P0 | Page render < 100 ms on a 500-page PDF on Apple silicon; smooth scrolling; zoom 25–400%. |
| FR-READ-02 | Render EPUBs with Readium/WebKit. | P0 | Chapters load < 300 ms; reflowable text; font size, line height, margins, theme. |
| FR-READ-03 | Navigate via outline, thumbnails, page number, and search results. | P0 | All navigation methods jump to the correct location. |
| FR-READ-04 | Search inside the current book. | P0 | Results show page/CFI and snippet; next/previous match; highlight matches. |
| FR-READ-05 | Keyboard navigation. | P0 | Arrow keys/Page Up/Down/Space; `⌘F` search; `⌘G` next match; `Esc` closes panels. |
| FR-READ-06 | Reading themes and typography. | P0 | Light, sepia, dark; font size 12–28; line height 1.2–2.0; serif/sans options for EPUB. |
| FR-READ-07 | Two-page PDF view. | P1 | Toggle single/two-page; remembers per book. |
| FR-READ-08 | Split view with notes/diagram. | P1 | Reader and notes/diagram can be shown side by side; state persists. |
| FR-READ-09 | Text-to-speech. | P2 | Uses macOS speech synthesis; highlights spoken text. |

### 9.3 Reading state

| ID | Requirement | Priority | Acceptance criteria |
|---|---|---|---|
| FR-STATE-01 | Remember PDF position (page + scroll offset + zoom). | P0 | Reopening returns to within 1 page and same zoom; works after restart. |
| FR-STATE-02 | Remember EPUB position (CFI/locator). | P0 | Reopening returns to the same paragraph; works after font-size changes. |
| FR-STATE-03 | “Continue Reading” shelf. | P0 | Shows last 3–5 books with progress and last-read time; updates within 1 second. |
| FR-STATE-04 | Mark as read/unread; progress percentage. | P0 | Manual override; progress computed from locator. |
| FR-STATE-05 | Sync reading position across Macs. | P1 | Last-write-wins with timestamp; conflict prompt if both changed within 5 minutes. |

### 9.4 Highlights and bookmarks

| ID | Requirement | Priority | Acceptance criteria |
|---|---|---|---|
| FR-ANN-01 | Highlight text in PDF and EPUB. | P0 | Selection → highlight in < 200 ms; color picker; undo. |
| FR-ANN-02 | Add a note to a highlight. | P0 | Note editor opens inline; saves on blur/⌘↵; appears in margin and Notes panel. |
| FR-ANN-03 | Bookmark a page/location. | P0 | `⌘⇧B` toggles; bookmark list per book; jump to bookmark. |
| FR-ANN-04 | Annotation sidebar. | P0 | Lists highlights, notes, bookmarks, and voice notes in reading order; filter by type/color. |
| FR-ANN-05 | Store annotations as sidecar data, not by modifying the original file. | P0 | Original file hash unchanged; optional “export annotated PDF” creates a copy. |
| FR-ANN-06 | Tags and colors. | P0 | 6 colors + custom tags; searchable. |
| FR-ANN-07 | Export highlights to Markdown/CSV/JSON. | P1 | Includes book, author, page/CFI, text, note, tags, date. |
| FR-ANN-08 | Import highlights from Readwise/Kindle. | P2 | Match by title/author; review before merge. |

### 9.5 Notes

| ID | Requirement | Priority | Acceptance criteria |
|---|---|---|---|
| FR-NOTE-01 | Create a text note attached to a book/page/highlight. | P0 | Markdown support; autosave; appears in Notes panel. |
| FR-NOTE-02 | Create a standalone “inbox” note. | P0 | Can later link to a book/passage. |
| FR-NOTE-03 | Edit and delete notes. | P0 | Undo; deleted notes go to Trash for 30 days. |
| FR-NOTE-04 | Backlinks. | P1 | Note shows “Referenced by” diagrams/notes. |
| FR-NOTE-05 | Note templates. | P2 | e.g., “Summary,” “Question,” “Quote,” “Critique.” |

### 9.6 Voice notes

| ID | Requirement | Priority | Acceptance criteria |
|---|---|---|---|
| FR-VOICE-01 | Record a voice note from the reader or globally. | P0 | `⌘⇧V` starts/stops; waveform; timer; cancel discards. |
| FR-VOICE-02 | On-device transcription. | P0 | Transcript appears within 2× recording duration; no network required when on-device model is available. |
| FR-VOICE-03 | Attach transcript to a book/page/highlight. | P0 | Source link displayed; jump from transcript to passage. |
| FR-VOICE-04 | Edit transcript. | P0 | Edits are saved; original audio retained unless user deletes it. |
| FR-VOICE-05 | Search transcripts. | P0 | Transcript text is indexed; results show matched snippet + timestamp. |
| FR-VOICE-06 | Language support. | P0 | English first; additional languages depend on Apple/Whisper model availability; show supported languages. |
| FR-VOICE-07 | Audio retention policy. | P0 | User chooses “keep audio,” “delete audio after transcription,” or “ask each time.” |
| FR-VOICE-08 | Fallback when on-device model unavailable. | P0 | Offer text note, local model download, or explicit cloud transcription (off by default, clearly labeled). |

**Technical note:** Prefer Apple’s on-device Speech framework (`requiresOnDeviceRecognition = true`) where the language/OS supports it. Use WhisperKit or another local model as a fallback. No audio or transcript leaves the device by default.

### 9.7 Diagrams

| ID | Requirement | Priority | Acceptance criteria |
|---|---|---|---|
| FR-DIAG-01 | Create a blank diagram canvas. | P0 | Add/rename/delete nodes; connect edges; drag; zoom; pan; undo/redo. |
| FR-DIAG-02 | Link nodes to a source. | P0 | Node citation picker searches books/highlights; citation chip opens the source. |
| FR-DIAG-03 | Generate a diagram from a note/passage. | P1 | Local model returns structured nodes/edges; preview before insert; editable afterward. |
| FR-DIAG-04 | Auto-layout. | P1 | Hierarchical and radial layouts; no overlapping nodes for < 50 nodes. |
| FR-DIAG-05 | Export. | P1 | PNG, SVG, PDF; includes citations optionally. |
| FR-DIAG-06 | Embed diagram in a note. | P1 | Diagram appears as a card; click opens canvas. |
| FR-DIAG-07 | Freeform drawing/ink. | P2 | Not required for 1.x. |

**Scope note:** This is a concept-map editor, not a whiteboard or vector illustration tool. Nodes are text-first and citation-first.

### 9.8 Search

| ID | Requirement | Priority | Acceptance criteria |
|---|---|---|---|
| FR-SEARCH-01 | Global command palette (`⌘K`). | P0 | Opens < 100 ms; keyboard navigation; `Esc` closes. |
| FR-SEARCH-02 | Full-text search across book text, highlights, notes, and transcripts. | P0 | Results in < 150 ms for 1M passages on Apple silicon; snippets with matched terms. |
| FR-SEARCH-03 | Fuzzy search for titles, authors, tags, and filenames. | P0 | Typo-tolerant; ranks exact matches first. |
| FR-SEARCH-04 | Filters and scopes. | P0 | Filter by type, book, tag, date, format, has-notes/has-voice/has-diagram. |
| FR-SEARCH-05 | Jump to exact location. | P0 | Opens book at page/CFI and scrolls to the passage; highlights the match temporarily. |
| FR-SEARCH-06 | Search history and saved searches. | P1 | Local only; clearable. |
| FR-SEARCH-07 | OCR scanned PDFs. | P1 | Vision OCR; index page text; show OCR confidence and “searchable” badge. |
| FR-SEARCH-08 | Semantic/hybrid search. | P1 | On-device embeddings; combine BM25 + cosine similarity; cite passages. |
| FR-SEARCH-09 | “Ask your library.” | P1 | Local LLM answers with citations; refuses when evidence is insufficient; no web. |
| FR-SEARCH-10 | Search diagram labels and notes. | P0 | Diagram node text and note bodies are indexed. |

**Search implementation:** SQLite FTS5 for lexical search, with a passage table keyed by book/locator. Use BM25 plus boosts for title, highlight, and recency. Add a native fuzzy command palette; do not shell out to `fzf`/`fff` per keystroke. The “fzf feel” comes from ranking and keyboard UX, not a subprocess.

### 9.9 AI assistance

| ID | Requirement | Priority | Acceptance criteria |
|---|---|---|---|
| FR-AI-01 | Summarize a selection, page, chapter, or book. | P1 | On-device only; output is editable; cites the source range. |
| FR-AI-02 | Generate a diagram from a note/passage. | P1 | Structured JSON; preview; editable; citations mandatory. |
| FR-AI-03 | Suggest tags/titles for notes. | P2 | User can accept/reject; no automatic changes. |
| FR-AI-04 | Ask library with citations. | P1 | Answers include passage citations; “not enough evidence” response when retrieval is weak. |
| FR-AI-05 | Model management. | P1 | Show which model is installed, size, and whether it runs on-device; allow delete. |
| FR-AI-06 | No silent cloud fallback. | P0 | Any network AI use requires explicit opt-in per feature. |

**Platform note:** Apple Foundation Models are used where available (macOS 26+, Apple silicon). On older systems, AI features are disabled or use an optional local model pack. Core reading/search works without AI.

### 9.10 Export and integrations

| ID | Requirement | Priority | Acceptance criteria |
|---|---|---|---|
| FR-EXPORT-01 | Export highlights/notes to Markdown. | P0 | One file per book or one combined file; frontmatter includes title/author. |
| FR-EXPORT-02 | Copy citation. | P0 | Format: title, author, page/CFI, publisher/year (if known). |
| FR-EXPORT-03 | Export diagram as PNG/SVG/PDF. | P1 | Transparent/white background options; citations optional. |
| FR-EXPORT-04 | Obsidian export. | P1 | Writes Markdown files to a chosen vault folder; configurable template. |
| FR-EXPORT-05 | Readwise export. | P1 | CSV/Markdown compatible with Readwise import. |
| FR-EXPORT-06 | Kindle/Kobo handoff. | P1 | Send to Kindle via email (SMTP to the user's `@kindle.com` address) and open `amazon.com/sendtokindle` for a manual upload; copy EPUB to Kobo via USB; no DRM handling. |
| FR-EXPORT-08 | File-to-Kindle converter. | P1 | PDF, TXT, MD and HTML are re-flowed into a Kindle-ready EPUB (cover from page 1, nav document, NCX, clean metadata); an EPUB input is normalised rather than rebuilt; the result is a valid EPUB 3 package that the app can read back. |
| FR-EXPORT-07 | Annotated PDF export. | P1 | Creates a copy with highlights/notes flattened; original untouched. |

### 9.11 Sync (1.1)

| ID | Requirement | Priority | Acceptance criteria |
|---|---|---|---|
| FR-SYNC-01 | Optional iCloud/CloudKit sync. | P1 | Off by default; user enables per library. |
| FR-SYNC-02 | Sync metadata, reading positions, annotations, notes, diagrams. | P1 | Changes appear on another Mac within 30 seconds on a good connection. |
| FR-SYNC-03 | Do not sync book files by default. | P1 | User can opt in to file sync; respects iCloud storage. |
| FR-SYNC-04 | Conflict resolution. | P1 | Last-write-wins for positions; merge for annotations; prompt for destructive conflicts. |
| FR-SYNC-05 | Status UI. | P1 | Shows last sync, pending changes, errors; manual “Sync now.” |

### 9.12 Settings, privacy, and permissions

| ID | Requirement | Priority | Acceptance criteria |
|---|---|---|---|
| FR-SET-01 | First-run onboarding. | P0 | Explains local-first model; asks for folder access; optional metadata lookup; optional microphone. |
| FR-SET-02 | Privacy dashboard. | P0 | Shows what is stored locally, what network calls are enabled, and how to delete data. |
| FR-SET-03 | Telemetry off by default. | P0 | Opt-in only; no book titles, text, or file paths in analytics. |
| FR-SET-04 | Delete library/annotations. | P0 | Clear confirmation; originals untouched. |
| FR-SET-05 | Keyboard shortcut customization. | P1 | Core shortcuts can be remapped. |
| FR-SET-06 | Backup/restore. | P1 | Export a Stacks archive (metadata + annotations, not books) and restore it. |

---

## 10. Non-functional requirements

### 10.1 Performance

- Cold launch: < 1.5 s on Apple silicon; < 3 s on Intel.
- Library scroll: 60 fps with 5,000 books.
- PDF page render: < 100 ms typical; < 250 ms for complex pages.
- EPUB chapter load: < 300 ms.
- Global search: p95 < 150 ms for 1M indexed passages.
- Highlight creation: < 200 ms perceived.
- Voice transcription: < 2× recording duration on Apple silicon.
- Memory: < 500 MB with a 1,000-book library and 3 open tabs; < 1.5 GB during OCR/AI jobs.

### 10.2 Reliability

- Crash-free sessions > 99.5%.
- No data loss on crash: annotations autosave within 1 second; write-ahead logging in SQLite.
- Index rebuild is resumable and never blocks reading.
- Graceful handling of missing files, corrupt PDFs, and unsupported EPUBs.

### 10.3 Privacy and security

- Local-first: book content, notes, voice audio, transcripts, and embeddings stay on device by default.
- Network calls only for:
  - metadata lookup (Open Library/Google Books), opt-in;
  - optional model downloads;
  - optional sync (CloudKit);
  - opt-in crash reports.
- No analytics SDKs that capture content or file paths.
- App Sandbox + hardened runtime + notarization for direct distribution.
- User-selected file access; no broad disk scanning without permission.
- Microphone permission requested only when recording.
- Speech recognition permission requested with clear explanation.
- No DRM circumvention.
- Optional encrypted local database (SQLCipher) as a Pro/advanced setting; evaluate performance cost.

### 10.4 Accessibility

- Full keyboard navigation.
- VoiceOver labels for library, reader, annotations, search, and diagram canvas.
- Dynamic Type / larger text for UI chrome (reader typography already adjustable).
- High-contrast mode.
- Reduce Motion respected.
- Captions/transcripts for voice notes.
- Focus indicators on all interactive elements.

### 10.5 Compatibility

- macOS 14 Sonoma+ (minimum).
- Apple silicon and Intel.
- AI features: macOS 26+ and Apple silicon recommended; gracefully disabled otherwise.
- 8 GB RAM minimum; 16 GB recommended for large libraries/OCR.
- Retina and external displays; multi-monitor window restoration.
- PDF 1.4–2.0; EPUB 2 and 3 (reflowable; fixed-layout best effort).
- No support for DRM-protected files.

### 10.6 Localization

- English UI at launch.
- Voice transcription languages depend on Apple/Whisper model support; list supported languages in Settings.
- UI localization (Spanish, French, German, Japanese, Chinese) after 1.0 based on demand.

---

## 11. Data model

All data is local SQLite unless sync is enabled.

### 11.1 Core tables

```sql
books (
  id TEXT PRIMARY KEY,
  title TEXT NOT NULL,
  author TEXT,
  publisher TEXT,
  year INTEGER,
  language TEXT,
  format TEXT NOT NULL,          -- pdf | epub
  file_path TEXT NOT NULL,
  file_hash TEXT NOT NULL,
  cover_path TEXT,
  page_count INTEGER,
  added_at TEXT NOT NULL,
  updated_at TEXT NOT NULL,
  last_opened_at TEXT,
  is_read INTEGER DEFAULT 0
)

collections (
  id TEXT PRIMARY KEY,
  name TEXT NOT NULL,
  created_at TEXT NOT NULL
)

collection_books (
  collection_id TEXT NOT NULL,
  book_id TEXT NOT NULL,
  PRIMARY KEY (collection_id, book_id)
)

tags (
  id TEXT PRIMARY KEY,
  name TEXT NOT NULL UNIQUE
)

book_tags (
  book_id TEXT NOT NULL,
  tag_id TEXT NOT NULL,
  PRIMARY KEY (book_id, tag_id)
)

reading_states (
  book_id TEXT PRIMARY KEY,
  locator TEXT NOT NULL,          -- page:... | cfi:...
  progress REAL DEFAULT 0,        -- 0..1
  zoom REAL,
  updated_at TEXT NOT NULL
)

annotations (
  id TEXT PRIMARY KEY,
  book_id TEXT NOT NULL,
  kind TEXT NOT NULL,             -- highlight | bookmark | note
  locator_start TEXT,
  locator_end TEXT,
  page INTEGER,
  text TEXT,
  note TEXT,
  color TEXT,
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL,
  deleted_at TEXT
)

notes (
  id TEXT PRIMARY KEY,
  book_id TEXT,
  annotation_id TEXT,
  title TEXT,
  body_markdown TEXT NOT NULL,
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL,
  deleted_at TEXT
)

voice_notes (
  id TEXT PRIMARY KEY,
  book_id TEXT,
  annotation_id TEXT,
  audio_path TEXT,
  duration_ms INTEGER,
  transcript TEXT,
  language TEXT,
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL
)

diagrams (
  id TEXT PRIMARY KEY,
  book_id TEXT,
  note_id TEXT,
  title TEXT NOT NULL,
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL
)

diagram_nodes (
  id TEXT PRIMARY KEY,
  diagram_id TEXT NOT NULL,
  label TEXT NOT NULL,
  body TEXT,
  x REAL NOT NULL,
  y REAL NOT NULL,
  color TEXT,
  source_annotation_id TEXT,
  source_book_id TEXT,
  source_locator TEXT
)

diagram_edges (
  id TEXT PRIMARY KEY,
  diagram_id TEXT NOT NULL,
  from_node_id TEXT NOT NULL,
  to_node_id TEXT NOT NULL,
  label TEXT
)

passages (
  id TEXT PRIMARY KEY,
  book_id TEXT NOT NULL,
  locator TEXT NOT NULL,
  page INTEGER,
  text TEXT NOT NULL,
  ocr INTEGER DEFAULT 0,
  created_at TEXT NOT NULL
)

passages_fts USING fts5(
  text,
  content='passages',
  content_rowid='rowid'
)
```

### 11.2 Optional sync tables

```sql
sync_events (
  id TEXT PRIMARY KEY,
  entity_type TEXT NOT NULL,
  entity_id TEXT NOT NULL,
  operation TEXT NOT NULL,        -- upsert | delete
  payload_hash TEXT,
  updated_at TEXT NOT NULL,
  synced_at TEXT
)
```

### 11.3 File storage

```
~/Library/Application Support/Stacks/
├── library.sqlite
├── covers/
├── audio/
├── diagrams/
├── models/          # optional local models
└── backups/
```

Original book files remain wherever the user stored them. A “managed library” mode can copy files into `books/` with explicit consent.

---

## 12. Technical architecture

### 12.1 Stack

- **UI:** SwiftUI + AppKit (NSWindow, NSSplitView, menus, keyboard shortcuts).
- **PDF:** PDFKit (`PDFView`, `PDFDocument`, `PDFSelection`, `PDFAnnotation`).
- **EPUB:** Readium Swift Toolkit, or WebKit + a custom EPUB renderer if Readium integration is too heavy.
- **Database:** SQLite + GRDB (or raw SQLite) with FTS5.
- **Search:** FTS5 BM25 + native fuzzy matcher; optional embeddings via NaturalLanguage/Core ML.
- **Voice:** AVFoundation for capture; Apple Speech (on-device) or WhisperKit for transcription.
- **AI:** Apple Foundation Models where available; optional local model via MLX/llama.cpp.
- **OCR:** Vision (`VNRecognizeTextRequest`).
- **Diagrams:** SwiftUI Canvas + custom node/edge model; export via Core Graphics/PDFKit.
- **Sync:** CloudKit (private database) in 1.1; local-first conflict handling.
- **Distribution:** Direct download (notarized, Sparkle updates) first; Mac App Store later if sandbox constraints allow.

### 12.2 Indexing pipeline

```
Import file
  → detect format
  → hash + dedupe
  → extract metadata
  → extract text per page/CFI
  → OCR if scanned (optional)
  → chunk into passages
  → write passages
  → update FTS5
  → optional embeddings
  → mark book indexed
```

Indexing runs in the background with progress UI. Reading is available before indexing completes. Reindex command per book/all books.

### 12.3 Search ranking

1. Exact phrase match in title/author → highest.
2. Exact phrase in passage → high.
3. BM25 relevance.
4. Boost if match is in a highlight/note/voice transcript.
5. Recency boost for recently opened books.
6. Type filter boosts (user-selected scope).
7. Fuzzy title/author fallback.

Snippet generation highlights matched terms and respects word boundaries.

### 12.4 AI grounding

- Retrieval: FTS5 + optional embeddings; top 8–12 passages.
- Generation: local model with a strict prompt:
  - answer only from supplied passages;
  - cite passage IDs;
  - say “not enough evidence” when retrieval is weak;
  - no web access.
- Output validation: every citation must map to a real passage; drop or flag invalid citations.
- User can inspect the retrieved passages.

### 12.5 Security boundaries

- App Sandbox enabled.
- No arbitrary shell execution.
- No network access unless a feature explicitly requires it; network client is centralized and auditable.
- Model downloads are signed/checksummed and user-initiated.
- Crash reports scrub file paths and book text.

---

## 13. Dependencies and constraints

### 13.1 External dependencies

- Apple PDFKit, Vision, Speech, AVFoundation, CloudKit.
- Readium Swift Toolkit (license audit required).
- GRDB or SQLite (license audit required).
- WhisperKit / MLX (optional; license and model size audit).
- Open Library and Google Books APIs for metadata (rate limits, attribution).
- Sparkle for direct updates (MIT).

### 13.2 Constraints

- No DRM circumvention.
- No guaranteed on-device transcription for every language.
- AI features depend on OS/hardware; must degrade gracefully.
- App Store sandbox may limit USB/Kindle handoff and folder watching; direct distribution is primary.
- Large libraries (10k+ books, 100k+ passages) require careful indexing and pagination.
- EPUB rendering is complex; fixed-layout and complex CSS may not render perfectly in 1.0.

---

## 14. Analytics and success metrics

### 14.1 Activation

- User imports ≥ 10 books within first 10 minutes.
- User opens a book and creates first highlight within first session.
- User returns within 7 days.

### 14.2 Engagement

- Weekly active readers / monthly active readers > 40%.
- Median sessions per active week ≥ 3.
- Voice notes used by ≥ 25% of active users.
- Diagrams used by ≥ 10% of active users.
- Search success rate (result opened) ≥ 70%.

### 14.3 Retention and conversion

- D30 retention ≥ 35%.
- Trial → paid conversion ≥ 3–5%.
- Refund rate < 5%.
- Crash-free sessions > 99.5%.

### 14.4 Quality

- p95 search latency < 150 ms.
- p95 PDF page render < 250 ms.
- Sync error rate < 1% (1.1).
- Support tickets per 100 users < 2/month.

### 14.5 Measurement approach

- Local, privacy-preserving counters only.
- Opt-in telemetry; no book titles, text, paths, or audio.
- In-app “Send diagnostics” exports logs with user review.
- Beta cohort interviews at weeks 2, 4, and 8.

---

## 15. Packaging and pricing

### 15.1 Model

- **Free trial:** 14 days or 50 books, whichever comes first. All core features.
- **Stacks:** $49 one-time (launch price; $69 regular).
- **Stacks Pro:** $89 one-time (launch; $119 regular) — includes OCR, auto-diagrams, semantic search, and 12 months of sync; sync renews at $19/year optional.
- **Education:** 50% off with verification.
- **Upgrade path:** Stacks → Pro at the price difference.

### 15.2 Rationale

- Local models eliminate per-query AI costs.
- One-time pricing is a differentiator against Readwise, DEVONthink, and subscription PDF tools.
- Sync has minor ongoing cost; a small optional renewal is fair.
- No ads, no data selling, no content-based upsells.

### 15.3 Licensing

- Per-user license, up to 3 Macs.
- Family/team licensing later.
- Direct license key + Mac App Store purchase (if available).
- 30-day money-back guarantee.

---

## 16. Go-to-market

### 16.1 Launch narrative

**“Calibre is a library. Stacks is a reading desk.”**

Supporting messages:
- “Every PDF and EPUB you own, instantly searchable.”
- “Speak a note without losing your place.”
- “Turn a passage into a diagram — with the citation attached.”
- “Local-first. No account. No subscription.”

### 16.2 Channels

- Product Hunt launch.
- Hacker News “Show HN.”
- Reddit: r/macapps, r/Calibre, r/ebooks, r/kindle, r/kobo, r/PhD, r/academia, r/lawschool, r/consulting.
- YouTube/TikTok demos: “I replaced Preview + Calibre + Voice Memos with one Mac app.”
- Newsletters: MacStories, Dense Discovery, Recomendo, Exploding Topics.
- SEO: “Calibre alternative Mac,” “PDF reader with voice notes,” “local-first Readwise alternative,” “search inside PDFs Mac.”
- Partnerships: Obsidian/Readwise export, academic tooling blogs.

### 16.3 Launch assets

- 60-second demo video.
- 5 GIFs: continue reading, voice note, diagram, global search, export.
- Landing page with waitlist and lifetime pre-sale.
- Comparison page vs Calibre, Apple Books, Readwise, DEVONthink.
- Privacy page: “Your books never leave your Mac.”

### 16.4 Pre-launch validation

- 20 interviews with heavy readers.
- 10 beta users with 100+ PDFs.
- Pre-sell 50 lifetime licenses at $29.
- Measure: do users import, read, highlight, and return in week 2?

---

## 17. Risks and mitigations

| Risk | Impact | Likelihood | Mitigation |
|---|---|---|---|
| Scope creep into a general notes app | High | High | Enforce “anchored notes” rule; no daily journal/tasks; roadmap gate. |
| EPUB rendering complexity | High | Medium | Start with reflowable EPUB; fixed-layout best effort; Readium; beta with diverse files. |
| On-device speech unavailable for some languages | Medium | Medium | WhisperKit fallback; clear language support; text notes always available. |
| Performance with 10k+ PDFs | High | Medium | Incremental indexing; FTS5; pagination; background OCR; benchmark early. |
| Sync conflicts/data loss | High | Medium | Local-first; WAL; conflict prompts; backup/export; ship sync after core is stable. |
| App Store sandbox limits | Medium | High | Direct distribution first; App Store later with reduced features if needed. |
| Legal/copyright concerns | High | Low | No DRM removal; user-owned files; no content sharing; clear terms. |
| Competition from Readwise/DEVONthink | Medium | Medium | Win on reading UX, local-first, voice/diagram integration, one-time price. |
| AI hallucination | High | Medium | Citations required; “not enough evidence”; retrieval inspection; no web. |
| Users expect Kindle/Apple Books DRM files to work | Medium | High | Clear onboarding: DRM-free PDF/EPUB only; explain why. |

---

## 18. Milestones

Assumes a small team (1–2 engineers + design/QA support).

| Milestone | Duration | Deliverable |
|---|---|---|
| M0 — Validation | 1 week | 20 interviews; landing page; 50-person waitlist; pricing test. |
| M1 — Foundation | 3 weeks | App shell, library DB, import PDF/EPUB, metadata, covers. |
| M2 — Reader | 4 weeks | PDF/EPUB rendering, navigation, position memory, bookmarks. |
| M3 — Annotations | 3 weeks | Highlights, notes, annotation sidebar, export Markdown. |
| M4 — Voice | 3 weeks | Recording, on-device transcription, attachment, transcript search. |
| M5 — Search | 3 weeks | FTS5 index, command palette, filters, jump-to-source, OCR spike. |
| M6 — Diagrams | 3 weeks | Canvas, nodes/edges, citations, export; auto-diagram prototype. |
| M7 — Beta | 4 weeks | 10–20 beta users, performance fixes, onboarding, crash reporting. |
| M8 — 1.0 | 2 weeks | Notarization, licensing, landing page, launch assets, release. |

**Target: 16–18 weeks from kickoff to 1.0.** If solo, expect 24–30 weeks; cut diagrams to 1.1 if needed.

---

## 19. Release criteria

### 19.1 1.0 must-haves

- Import 100 PDFs and 50 EPUBs without crashes.
- Read, navigate, and remember position across restarts.
- Highlight, bookmark, and note in both formats.
- Record a voice note, transcribe on-device, and find it via search.
- Global search returns results in < 150 ms for a 1,000-book library.
- Create a diagram with cited nodes and export PNG/SVG.
- Export highlights to Markdown.
- No known data-loss bugs.
- Crash-free beta sessions > 99%.
- 10 beta users complete the core loop without assistance.

### 19.2 1.0 nice-to-haves

- OCR for scanned PDFs.
- Auto-diagram from note.
- Kindle/Kobo handoff, including the file-to-Kindle converter (built early in the prototype).
- Readwise/Obsidian export.
- iCloud sync beta.

### 19.3 Quality gates

- Performance benchmarks pass on M1 MacBook Air (8 GB) and Intel Mac (if supported).
- Accessibility audit with VoiceOver.
- Security review: network calls, sandbox, file access, model downloads.
- Legal review: DRM, copyright, third-party licenses.
- Backup/restore tested.

---

## 20. Open questions

1. **Minimum OS:** macOS 14 or 15? macOS 14 broadens reach; macOS 26 unlocks Apple Foundation Models.
2. **EPUB engine:** Readium vs custom WebKit renderer. Which gives better annotation fidelity and lower binary size?
3. **App Store:** Is direct distribution enough for 1.0, or does App Store presence matter for trust?
4. **Sync:** Ship local-only 1.0 and add CloudKit in 1.1, or include sync beta at launch?
5. **AI model:** Apple Foundation Models only, or bundle an optional local model for older Macs?
6. **Pricing:** $49 vs $69 launch; Pro at $89 vs $99.
7. **Diagram scope:** Is a text-first concept map enough, or do users expect freeform drawing?
8. **Voice:** Is on-device Apple Speech accurate enough for accented English and technical vocabulary? Benchmark early.
9. **Library management:** Keep files in place by default, or offer managed library import?
10. **Calibre import:** How much metadata compatibility is required for switchers?

---

## Appendix A — Keyboard shortcuts

| Shortcut | Action |
|---|---|
| `⌘K` | Global search / command palette |
| `⌘⇧F` | Search inside current book |
| `⌘⇧L` | Library |
| `⌘⇧V` | Record voice note |
| `⌘⇧N` | New text note |
| `⌘⇧H` | Highlight selection |
| `⌘⇧B` | Toggle bookmark |
| `⌘⇧D` | Create diagram from selection/note |
| `⌘G` | Next search match |
| `⌘⇧G` | Previous search match |
| `Space` | Page down (reader) |
| `⇧Space` | Page up (reader) |
| `⌘+` / `⌘−` | Zoom in/out |
| `⌘0` | Fit page/width |
| `Esc` | Close panel/popover |

---

## Appendix B — Glossary

- **CFI:** Canonical Fragment Identifier; a locator for a position in an EPUB.
- **FTS5:** SQLite’s full-text search extension.
- **OCR:** Optical character recognition; converts scanned page images into searchable text.
- **Local-first:** Data is stored and processed on the user’s device by default; sync is optional.
- **Passage:** A chunk of book text indexed for search and citation.
- **Anchored note:** A note linked to a book, page, CFI, highlight, or timestamp.
- **On-device model:** A model that runs locally on the Mac, not on a remote server.

---

## Appendix C — Example user stories

### C.1 Resume reading

> As a researcher, I want Stacks to reopen the exact page and scroll position of my last book so I can continue without searching.

**Acceptance:** Open app → “Continue Reading” card → click → PDF opens at the same page, scroll offset, and zoom; EPUB opens at the same CFI.

### C.2 Voice note

> As a reader, I want to speak a thought while reading and have it attached to the passage so I can find it later.

**Acceptance:** Select text → `⌘⇧V` → speak → stop → transcript appears → save → note appears in sidebar and search results with a link to the passage.

### C.3 Search across everything

> As a knowledge worker, I want to search “pricing power” and see book passages, my highlights, my voice notes, and diagram nodes in one list.

**Acceptance:** `⌘K` → type query → grouped results → `Return` opens the exact source location; results include snippets and type badges.

### C.4 Diagram with citations

> As a consultant, I want to turn a set of notes into a diagram and keep the source citations so I can defend my synthesis.

**Acceptance:** Select note → `⌘⇧D` → preview nodes/edges → confirm → canvas opens → each node has a citation chip → export PNG with citations.

### C.5 Privacy

> As a lawyer, I want to be certain my client documents are not uploaded to a cloud AI service.

**Acceptance:** Settings → Privacy shows “Local-only” for reading, search, voice, and AI; network log shows no content requests; cloud features are off by default.

---

*End of document.*
