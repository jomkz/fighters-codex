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
; optional comment lines
[blank lines]
<TAB><time><TAB><command>[<TAB><arg1> <arg2> ...]<CR><LF>
```

- Lines beginning with `;` are comments and are preserved verbatim on round-trip.
- Event lines begin with a tab, then a time field, then another tab, then a command.
- Lines may use `\r\n` or `\n` — write `\r\n` on output.

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

### Known commands

| Command | Typical args | Notes |
|---------|-------------|-------|
| `bitmap` | `"NAME" x y flags width` | Display image at (x,y) |
| `palette` | `"NAME"` | Load a named palette |
| `font` | `"NAME"` | Set current font |
| `video` | `"NAME"` | Play video clip |
| `sound` | `"NAME"` | Play sound (quoted, no extension; `^` prefix = looping) |
| `fadein` | `seconds` | Fade to full brightness |
| `fadeout` | `seconds` | Fade to black |
| `wait` | (none) | Pause until sound/video completes |
| `sync` | sub-command... | Execute sub-command synchronously |

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

## Related

**Formats:** [11K](11K.md) — the `sound` command plays PCM clips (`^` prefix
= looping); [MUS](MUS.md) — the engine's `_SEQmusic` path triggers music
slots from sequence state.
