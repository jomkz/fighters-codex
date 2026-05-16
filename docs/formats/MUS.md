# Music Playlist / Sequencer (.MUS)

FA_2.LIB contains 9 `.MUS` files (e.g. `M_AIR.MUS`). These control background music playback. Each is a **DOS MZ executable overlay** loaded by the FA engine at runtime.

## Format

DOS MZ executable (magic `4D 5A`). All observed `.MUS` files decompressed to **4608 bytes**.

```
Offset  Value   Description
------  -----   -----------
0x00    4D 5A   MZ magic
0x02    80 00   Last page bytes used (128)
0x04    01 00   Pages in file
...
0x3C    80 00   Overlay header offset
```

The `.XMI` files (78 entries) are the actual audio sequences; `.MUS` overlays likely serve as playlists or state machines that reference `.XMI` tracks by name and define playback order, looping, and transition rules.

## Location

| LIB | Count |
|-----|-------|
| FA_2.LIB | 9 |

## TODO — Deep Dive

- Disassemble a `.MUS` overlay to confirm it acts as an `.XMI` playlist/sequencer
- Identify the `.XMI` track names referenced and the triggering conditions

## Related

- [XMI.md](XMI.md) — Extended MIDI audio tracks played by the music system
- [SEQ.md](SEQ.md) — cutscene sequencer, which may also trigger music state changes
