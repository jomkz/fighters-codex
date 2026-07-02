---
format: XMI
name: Extended MIDI Audio
extensions: [".XMI"]
category: audio
endianness: big
spec:
  status: stub
  gaps:
    - kind: re-static
      issue: 54
      note: "EVNT/TIMB chunk internals not specified"
codec:
  direction: none
  issue: 106
  gui: [gui/src/editors/xmi_editor.cpp]
  fixtures:
    synthetic: false
    real_manifest: true
related: [MUS, 11K]
---

# XMI — Extended MIDI Audio (.XMI)

FA_2.LIB contains 78 `.XMI` files. XMI is Miles Sound System's Extended MIDI
format — a compact IFF-based variant of Standard MIDI that supports multiple
independent sequences in a single file. Confirmed by hex analysis: magic
`FORM...XDIR`. All 78 are in FA_2.LIB only (FA_1.LIB contains none; FA_3.LIB
is disc-2 content).

## Tools

### Other Tools

Conversion to Standard MIDI (`.MID`) is possible with `xmi2mid` from the
WildMIDI project or similar tools (an `fx` exporter is tracked in #106).

## File Layout

IFF-style chunk structure — **chunk sizes are big-endian**, unlike every other
FA format. Well-documented external format.

| Offset | Chunk | Description |
|--------|-------|-------------|
| `0x00` | FORM | IFF outer envelope |
| `0x04` | (size) | u32 BE: total content size |
| `0x08` | XDIR | Extended MIDI directory marker |
| `0x0C` | INFO | Sequence count block |
| — | CAT | Sequence catalog |
| — | FORM XMID | One entry per MIDI sequence |
| — | TIMB | Instrument/timbre table |
| — | EVNT | MIDI event stream (AIL delta encoding) |

Key differences from Standard MIDI:
- Fixed 120 BPM base; tempo encoded as AIL-specific multipliers
- Delta times use AIL's variable-length encoding (not SMF)
- Multiple sequences in one file via the `CAT`/`XMID` structure

## Open Questions

### 1. EVNT and TIMB chunk internals

The chunk-level outline above is coarse: the TIMB instrument table layout and
the AIL delta encoding inside EVNT are documented externally (Miles Sound
System) but not yet specified here from FA's own files. Closing this means
documenting exactly what FA's engine consumes, verified against the 78-file
corpus.

*Status: open — re-static (#54)*

## Related

**Formats:** [MUS](MUS.md) — playlist DLLs that select XMI tracks by index;
[11K](11K.md) — PCM audio formats (`.5K`, `.11K`) used for sound effects and
voice.
