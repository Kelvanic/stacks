#!/usr/bin/env python3
"""Generate the small, original sample library that ships with the Stacks prototype."""

from __future__ import annotations

import html
import os
import zipfile
from pathlib import Path

from reportlab.lib import colors
from reportlab.lib.pagesizes import A5
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import mm
from reportlab.platypus import PageBreak, Paragraph, SimpleDocTemplate, Spacer

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets" / "samples"
OUT.mkdir(parents=True, exist_ok=True)


def make_pdf(path: Path, title: str, author: str, chapters: list[tuple[str, list[str]]]) -> None:
    styles = getSampleStyleSheet()
    body = ParagraphStyle(
        "body",
        parent=styles["BodyText"],
        fontName="Times-Roman",
        fontSize=10.5,
        leading=16,
        textColor=colors.HexColor("#201C16"),
        spaceAfter=10,
    )
    chapter = ParagraphStyle(
        "chapter",
        parent=styles["Heading1"],
        fontName="Times-Bold",
        fontSize=18,
        leading=22,
        textColor=colors.HexColor("#201C16"),
        spaceBefore=18,
        spaceAfter=18,
    )
    title_style = ParagraphStyle(
        "title",
        parent=styles["Title"],
        fontName="Times-Bold",
        fontSize=34,
        leading=38,
        textColor=colors.HexColor("#201C16"),
    )
    author_style = ParagraphStyle(
        "author",
        parent=styles["Normal"],
        fontName="Times-Italic",
        fontSize=13,
        leading=18,
        textColor=colors.HexColor("#6F665A"),
    )
    note_style = ParagraphStyle(
        "note",
        parent=body,
        fontName="Times-Italic",
        textColor=colors.HexColor("#6F665A"),
        leftIndent=18,
        borderPadding=8,
    )

    doc = SimpleDocTemplate(
        str(path),
        pagesize=A5,
        leftMargin=22 * mm,
        rightMargin=22 * mm,
        topMargin=24 * mm,
        bottomMargin=22 * mm,
        title=title,
        author=author,
    )
    story = [Spacer(1, 46 * mm), Paragraph(title, title_style), Spacer(1, 6 * mm), Paragraph(author, author_style), PageBreak()]
    for heading, paragraphs in chapters:
        story.append(Paragraph(heading, chapter))
        for text in paragraphs:
            style = note_style if text.startswith("NOTE:") else body
            story.append(Paragraph(text, style))
        story.append(PageBreak())
    doc.build(story)


def make_epub(path: Path, title: str, author: str, chapters: list[tuple[str, list[str]]]) -> None:
    uid = "urn:uuid:stacks-" + "".join(c.lower() if c.isalnum() else "-" for c in title).strip("-")
    manifest = []
    spine = []
    nav_items = []
    files: dict[str, str] = {}

    for index, (heading, paragraphs) in enumerate(chapters):
        item_id = f"ch{index + 1}"
        href = f"text/{item_id}.xhtml"
        manifest.append(
            f'<item id="{item_id}" href="{href}" media-type="application/xhtml+xml"/>'
        )
        spine.append(f'<itemref idref="{item_id}"/>')
        nav_items.append(f'<li><a href="{href}">{html.escape(heading)}</a></li>')
        body = "\n".join(f"<p>{html.escape(p)}</p>" for p in paragraphs)
        files[href] = f'''<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en">
<head><title>{html.escape(heading)}</title><meta charset="utf-8"/></head>
<body>
  <h1>{html.escape(heading)}</h1>
  {body}
</body>
</html>'''

    opf = f'''<?xml version="1.0" encoding="utf-8"?>
<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="bookid">
  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
    <dc:identifier id="bookid">{uid}</dc:identifier>
    <dc:title>{html.escape(title)}</dc:title>
    <dc:creator>{html.escape(author)}</dc:creator>
    <dc:language>en</dc:language>
  </metadata>
  <manifest>
    <item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>
    {''.join(manifest)}
  </manifest>
  <spine>
    {''.join(spine)}
  </spine>
</package>'''

    nav = f'''<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops">
<head><title>Contents</title></head>
<body>
  <nav epub:type="toc"><h1>Contents</h1><ol>{''.join(nav_items)}</ol></nav>
</body>
</html>'''

    container = '''<?xml version="1.0" encoding="utf-8"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
  <rootfiles>
    <rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/>
  </rootfiles>
</container>'''

    with zipfile.ZipFile(path, "w") as archive:
        archive.writestr("mimetype", "application/epub+zip", compress_type=zipfile.ZIP_STORED)
        archive.writestr("META-INF/container.xml", container, compress_type=zipfile.ZIP_DEFLATED)
        archive.writestr("OEBPS/content.opf", opf, compress_type=zipfile.ZIP_DEFLATED)
        archive.writestr("OEBPS/nav.xhtml", nav, compress_type=zipfile.ZIP_DEFLATED)
        for href, contents in files.items():
            archive.writestr(f"OEBPS/{href}", contents, compress_type=zipfile.ZIP_DEFLATED)


attention = [
    ("The Practice", [
        "Attention is a practice, not a possession. It is rebuilt every morning, in small increments, by the ordinary decision to stay with one thing for another minute.",
        "A reading desk should disappear. The page, the light, and the next sentence are the whole interface. Everything else is a tax on thought.",
        "NOTE: The margin is where thinking happens. A book that cannot hold a margin is a book that cannot hold a conversation.",
        "We mistake speed for fluency. Fluency is the ability to slow down without losing the thread. A good reader is not a fast reader; a good reader is a reader who can return.",
        "The first mark in a book is an act of ownership. The second mark is an act of friendship. By the third, the reader and the book have begun to argue.",
    ]),
    ("The Desk", [
        "There is a particular silence that arrives when a room is arranged for reading. It is not the absence of sound. It is the absence of decisions.",
        "Put the book where the light is. Put the pencil where the hand is. Put everything else away. This is not minimalism as an aesthetic; it is minimalism as a courtesy to the next thought.",
        "NOTE: A reading app should remember the page, not the user. Memory is a feature; surveillance is a business model.",
        "The best tools for thought are the ones that let you forget them. They wait at the edge of the page, quiet and exact, until the moment you need to leave a mark.",
        "A library is not a list. It is a set of promises you have made to your future self. The app's job is to keep those promises visible without making them heavy.",
    ]),
    ("The Margin", [
        "A highlight is a small claim: this mattered. A note is a larger claim: this mattered to me, here, for this reason. A voice note is the largest claim of all: I was here, and I sounded like this.",
        "The margin is the only part of the book that belongs to the reader. It should feel like paper, not like a form. It should accept a thought before asking for a category.",
        "NOTE: Search is not a feature. Search is the proof that the margin was real.",
        "When you return to a book after a month, you do not want a dashboard. You want the sentence you underlined, the reason you underlined it, and the feeling of the room where you did it.",
        "Diagrams begin as fragments. A phrase, a page number, a question. The canvas is a way of holding them close enough to see the shape they make.",
    ]),
    ("The Return", [
        "Reading is cyclical. We return to the same passages at different ages and find different books inside them. The text has not changed. The margin has.",
        "A position is a promise. The app remembers where you stopped, not to trap you in a streak, but to make the return cheap.",
        "NOTE: The fastest interface is the one that is already open.",
        "The end of a book is not the end of the reading. It is the moment the margin becomes a map. The notes begin to point at one another. The diagrams begin to breathe.",
        "Close the book. Leave the desk as it is. Tomorrow, the sentence will still be there, and so will you.",
    ]),
]

quiet = [
    ("A Smaller Surface", [
        "Quiet software does not announce itself. It opens, it waits, it remembers. The best compliment it can receive is that nobody noticed it was there.",
        "Every toolbar is a small argument about what matters. Most apps make too many arguments. A reading app should make one: the page matters.",
        "NOTE: The fastest animation is the one you do not render.",
        "A window is a room. Corners, light, and proportion are not decoration; they are the physics of attention. A harsh edge makes a harsh thought.",
    ]),
    ("Local First", [
        "Local-first is not a sync strategy. It is a promise that the thing you made will still be yours when the network is gone.",
        "The database should fit in a pocket. The index should be a tool, not a service. The voice should be transcribed on the machine that heard it.",
        "NOTE: A book should open in less time than it takes to doubt whether it will.",
        "Speed is a form of respect. When an app hesitates, it asks the reader to hold two thoughts at once: the thought they had, and the thought that the app might be about to work.",
    ]),
    ("The Shape of a Day", [
        "Mornings are for reading; afternoons are for marking; evenings are for the map. This is not a productivity system. It is a rhythm that leaves room for surprise.",
        "A good day ends with fewer open loops than it began with, but not zero. The remaining loops are the interesting ones.",
        "NOTE: Keep the next page ready. That is most of the work.",
    ]),
]

systems = [
    ("The Room That Listens", [
        "A system that breathes has inputs, pauses, and an output that can be read by a human. Most systems have inputs and outputs and no pauses. They are not systems; they are sieves.",
        "The first design decision is where to put the silence. In a reading room, the silence goes around the page. In a diagram, it goes between the nodes.",
        "NOTE: A voice note is a pause made audible.",
        "The room does not need to understand you. It needs to hold what you said until you are ready to understand it.",
    ]),
    ("Small Machines", [
        "A small machine is one you can explain in a sentence. A text index is a small machine: split the page into passages, count the words, keep a map. The map is the magic.",
        "Search should feel like remembering, not like querying. You type three words, and the book opens at the sentence you had almost lost.",
        "NOTE: The best index is the one that returns the passage before you finish typing the question.",
        "The margin and the index are the same idea seen from two directions. One is for the reader in the moment; the other is for the reader who returns.",
    ]),
    ("The Map", [
        "A diagram is a sentence with its bones showing. The nodes are the nouns; the edges are the verbs. If the verbs are weak, the diagram is a list.",
        "Start with three nodes. One from the beginning of the book, one from the middle, one from the end. Connect them only if the connection can be said out loud.",
        "NOTE: A map that cannot be explained is a decoration.",
        "The point of the canvas is not to be complete. The point is to make the next question visible.",
    ]),
    ("The Return", [
        "A system that breathes leaves room for the reader to change. The notes from last year are not a verdict. They are a previous version of a conversation.",
        "When you reopen the book, the app should show you the margin before it shows you the menu. The margin is the history of your attention.",
        "NOTE: The best memory is the one that arrives without being asked.",
        "Close the loop. Open the next book. The room is still listening.",
    ]),
]

shape = [
    ("Before the Light", [
        "The day begins before the light does. There is a blue hour in which the desk is only a shape and the book is only a weight in the hand.",
        "I read the same page twice in the blue hour. The first time is for the words; the second is for the room they make.",
        "NOTE: Some pages are doors. You do not know which ones until you are already through.",
        "The cat has opinions about the chair. The chair has no opinions about the cat. This is the first lesson in attention.",
    ]),
    ("The Middle Hours", [
        "By noon the margin is full of small arrows. Each arrow is a promise to come back. Most promises are kept. The ones that are not become the diagram.",
        "A day has a texture. Some hours are paper; some are glass. The work is to notice which one you are standing on.",
        "NOTE: Take the note before you know what it means.",
        "The light moves across the desk. The page does not. This is the whole drama of reading, and it is enough.",
    ]),
    ("After the Lamp", [
        "At night the desk becomes an island. The lamp makes a small weather. The book is warm from the day.",
        "I close the cover and leave one sentence open in my head. It is not the most important sentence. It is the one that will still be there tomorrow.",
        "NOTE: End before you are empty. Leave something for the return.",
        "The day is a shape. It is not a line. It bends back toward the morning, and the morning is already different.",
    ]),
]


def main() -> None:
    make_pdf(OUT / "the-art-of-attention.pdf", "The Art of Attention", "Mira Chen", attention)
    make_pdf(OUT / "field-notes-on-quiet-software.pdf", "Field Notes on Quiet Software", "Jon Bell", quiet)
    make_epub(OUT / "systems-that-breathe.epub", "Systems That Breathe", "Mira Chen", systems)
    make_epub(OUT / "the-shape-of-a-day.epub", "The Shape of a Day", "Ari Vale", shape)
    print("wrote", sorted(p.name for p in OUT.iterdir()))


if __name__ == "__main__":
    main()
