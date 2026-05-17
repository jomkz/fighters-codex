# Music Playlist / Sequencer (.MUS)

FA_2.LIB contains 9 `.MUS` files (e.g. `M_AIR.MUS`). These control background music playback. Each is a **Win32 PE DLL** loaded at runtime via `LoadLibrary`.

## Format

Win32 PE DLL. All observed `.MUS` files decompressed to **4608 bytes**. String analysis of `M_AIR.MUS` yields only the standard PE header strings — no embedded `.XMI` track names are visible as plain text, suggesting XMI references are encoded or resolved at runtime by the engine rather than embedded in the DLL data section.

The `.XMI` files (78 entries in FA_2.LIB) are the actual audio sequences; `.MUS` overlays likely implement state machine logic that triggers XMI playback based on game state (in-air, in combat, in-base, etc.).

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
