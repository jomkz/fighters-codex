---
format: SEQ
name: Cutscene Timeline
extensions: [".SEQ"]
category: video
endianness: none
spec:
  status: complete
codec:
  direction: round-trip
  byte_identical: true
  lib: [lib/src/seq.cpp]
  commands: [seq]
  tests: [tests/test_seq.cpp]
  fuzz: [fuzz/fuzz_seq.cpp]
  gui: [gui/src/editors/seq_editor.cpp]
  fixtures:
    synthetic: true
    real_manifest: true
    real_install: true
related: [11K, MUS]
---

# SEQ — Cutscene Timeline (.SEQ)

SEQ files drive in-game cutscenes (mission briefings, death screens, campaign
intros). Each file is a plain ASCII text timeline: a sequence of timestamped
commands that trigger bitmaps, sounds, palette changes, and fades. FA_2.LIB
carries 126 of them.

## Tools

### fx

```
fx seq dump   <file.SEQ>              # pretty-print events to stdout
fx seq unpack <file.SEQ> [-o out.txt] # write editable text
fx seq pack   <in.txt>   -o <out.SEQ> # write binary SEQ
```

### Other Tools

SEQ files are plain ASCII — open and edit directly, no conversion step needed.

- **VS Code** — free; multi-file find/replace useful for batch renaming bitmap or sound references
- **Notepad++** — free, Windows; column editing helps with tab-aligned time fields
- **Notepad / TextEdit** — free, built-in; sufficient for small edits

## File Layout

Plain text; no binary fields.

```
; optional comment lines            (or //)
[blank lines]
<INDENT><time>[ sync]<TAB><command>[<TAB><arg1> <arg2> ...]<CR><LF>
```

- A line is a **comment** when it opens with `;` **or `//`** (`SeqSkipComments`, 0x445440).
- An event line is **indented with spaces or a tab** — the engine skips both alike before the
  content. 530 shipped events use a tab, and **three** (`UDEAD.SEQ`, `UWON.SEQ`, `ULOST.SEQ`)
  indent their final `fadeout` with **six spaces**.

  > `fx` used to require a tab, so it classified those three lines as comments and **dropped
  > the event** — the fadeout vanished from the timeline. All three files still round-tripped
  > byte-identically, because an unrecognised line is re-emitted verbatim. See
  > [#491](https://github.com/jomkz/fighters-codex/issues/491).

- Lines may use `\r\n` or `\n` — write `\r\n` on output.
- A trailing `0x1A` (DOS EOF) is preserved verbatim; 7 shipped sequences carry one.

### Time field

| Form | Meaning |
|------|---------|
| `0` | Absolute tick 0 |
| `5` | Absolute tick 5 |
| `+23` | Relative: 23 ticks after the previous event |

Trailing spaces after the time value (before the second tab) are legal and
common.

### Command and arguments

Arguments are space-separated on the same line, after the command:
- Quoted strings: `"NAME"` (filename references, no extension)
- Bare numbers / floats: `256`, `.5`, `0`

### Commands — the symbol table *is* the vocabulary

A SEQ command is an **import**. `_SeqContinue` (0x445700) builds `"_SEQ" + <command>`, resolves
it with **`SMAddress`**, and calls whatever comes back — the same mechanism BRF's `symbol`
keyword uses ([BRF.md](BRF.md)). That is why `fadeout` appears **nowhere in FA.EXE**: the
string that exists is `_SEQfadeout`, in **FA.SMS**. A command that does not resolve simply
does nothing.

So the vocabulary is not a guess — it is whatever `_SEQ*` symbols the engine exports, and
`tests/test_seq.cpp` checks every command in every shipped sequence against `db/symbols/`.

| Command | Symbol | Typical args | Notes |
|---------|--------|-------------|-------|
| `bitmap` | `_SEQbitmap` | `"NAME" x y flags width` | Display image at (x,y) |
| `palette` | `_SEQpalette` | `"NAME"` | Load a named palette |
| `font` | `_SEQfont` | `"NAME"` | Set current font |
| `video` | `_SEQvideo` | `"NAME"` | Play video clip |
| `sound` | `_SEQsound` | `"NAME"` | Play sound (quoted, no extension; `^` prefix = looping) |
| `fadein` | `_SEQfadein` | `seconds` | Fade to full brightness |
| `fadeout` | `_SEQfadeout` | `seconds` | Fade to black |
| `wait` | `_SEQwait` | (none) | Pause until sound/video completes |
| `text` | `_SEQtext` | — | **Exported, unused by any shipped sequence** |
| `music` | `_SEQmusic` | — | Exported, unused — see [MUS](MUS.md) |
| `sndoff` | `_SEQsndoff` | — | Exported, unused |
| `call` | `_SEQcall` | — | Exported, unused |
| `run` | `_SEQrun` | — | Exported, unused |

**`sync` is not a command** — it is a modifier that may precede one (`+23 sync fadeout .5`).
An earlier version of this table listed it as a command and omitted the five above.

### Includes

`include "NAME"` is part of the language (`SeqParseInclude`, 0x4456B0), alongside macro
substitution (`SeqSubstitute`, 0x4454D0). **No shipped sequence uses either**, and `fx` does
not expand them.

### Example

`KDEAD.SEQ` (92 bytes):

```
	0	bitmap "KDEAD" 0 0 0 256
	0 	fadein  .5
	0	sound "^KDEAD.11K"
	+23 sync	fadeout	.5
```

## Round-Trip Notes

`fx seq pack` emits files byte-identical to the originals (tabs, trailing
spaces, CRLF); `tests/test_seq.cpp` asserts it, including comment and
blank-line preservation. Parsed event count for KDEAD.SEQ: 4 events.

`tests/test_seq.cpp` also runs a census over **all 126 shipped sequences** (under
`FX_FA_ROOT`): each round-trips, and every one of the **533 events** decodes to a command that
resolves as a `_SEQ*` symbol in `db/symbols/`. The round-trip on its own proved none of that —
`seq_serialize` replays each line's own bytes, so a line it failed to understand round-tripped
exactly as well as one it did, which is how three events sat undecoded behind a green suite.

Census: `126 sequences, 533 events (3 space-indented, 166 sync, 62 relative);
bitmap=62 fadein=90 fadeout=175 font=28 palette=28 sound=45 video=103 wait=2`

## Related

**Formats:** [11K](11K.md) — the `sound` command plays PCM clips (`^` prefix
= looping); [MUS](MUS.md) — the engine's `_SEQmusic` path triggers music
slots from sequence state.
