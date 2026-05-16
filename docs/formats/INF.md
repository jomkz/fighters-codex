# Technical Info -- Aircraft Tech Sheet (.INF)

`.INF` files are **Rich Text Format (RTF)** documents containing the technical information
sheet for an aircraft. They are displayed in-game on the aircraft selection screen and in
the mission planner.

Found in: individual aircraft records inside the numbered `.LIB` files. One `.INF` per
aircraft; not all aircraft have one.

---

## Format

Standard RTF. No FA-specific extensions. Editable directly in WordPad, Microsoft Word,
or any RTF-capable editor.

## Extraction and Replacement

Extract via `ft lib unpack` (the record is stored uncompressed). Replace by packing the
edited `.RTF`/`.INF` back into a custom `FA_0.LIB` using `ft lib pack`.

When authoring new content, save as RTF 1.x. The in-game renderer is the Windows 3.1
RichEdit control and does not support advanced RTF features (embedded images, tables,
complex styles). Stick to plain text with basic formatting (bold, italic, font size).
