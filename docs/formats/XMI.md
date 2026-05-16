# Extended MIDI Audio (.XMI)

FA_2.LIB contains 78 `.XMI` files. XMI is Miles Sound System's Extended MIDI format — a compact IFF-based variant of Standard MIDI that supports multiple independent sequences in a single file. Confirmed by hex analysis: magic `FORM...XDIR`.

## Format

IFF-style chunk structure (big-endian chunk sizes). Well-documented external format.

```
Offset  Chunk     Description
------  -----     -----------
0x00    FORM      IFF outer envelope
0x04    (size)    u32 BE: total content size
0x08    XDIR      Extended MIDI directory marker
0x0C    INFO      Sequence count block
0x??    CAT       Sequence catalog
0x??    FORM XMID One entry per MIDI sequence
0x??    TIMB      Instrument/timbre table
0x??    EVNT      MIDI event stream (AIL delta encoding)
```

Key differences from Standard MIDI:
- Fixed 120 BPM base; tempo encoded as AIL-specific multipliers
- Delta times use AIL's variable-length encoding (not SMF)
- Multiple sequences in one file via the `CAT`/`XMID` structure

Conversion to Standard MIDI (`.MID`) is possible with `xmi2mid` from the WildMIDI project or similar tools.

## Location

| LIB | Count |
|-----|-------|
| FA_2.LIB | 78 |

## Related

- [MUS.md](MUS.md) — `.MUS` overlay files that likely act as playlists referencing `.XMI` tracks
- [AUDIO.md](AUDIO.md) — PCM audio formats (`.5K`, `.11K`) used for sound effects and voice
