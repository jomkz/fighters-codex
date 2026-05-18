# SEQ -- Sequence / Cutscene Timeline (.SEQ)

## Overview

SEQ files drive in-game cutscenes (mission briefings, death screens, campaign intros).
Each file is a plain ASCII text timeline: a sequence of timestamped commands that trigger
bitmaps, sounds, palette changes, and fades.

## File Layout

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

Trailing spaces after the time value (before the second tab) are legal and common.

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

## Example

`KDEAD.SEQ` (92 bytes):

```
	0	bitmap "KDEAD" 0 0 0 256
	0 	fadein  .5
	0	sound "^KDEAD.11K"
	+23 sync	fadeout	.5
```

## Round-Trip Notes

`ft seq pack` emits files byte-identical to the originals (tabs, trailing spaces, CRLF).
Parsed event count for KDEAD.SEQ: 4 events.

## ft commands

```
ft seq dump   <file.SEQ>              # pretty-print events to stdout
ft seq unpack <file.SEQ> [-o out.txt] # write editable text
ft seq pack   <in.txt>   -o <out.SEQ> # write binary SEQ
```

## Applications

SEQ files are plain ASCII — open and edit directly, no conversion step needed.

- **VS Code** — free; multi-file find/replace useful for batch renaming bitmap or sound references
- **Notepad++** — free, Windows; column editing helps with tab-aligned time fields
- **Notepad / TextEdit** — free, built-in; sufficient for small edits
