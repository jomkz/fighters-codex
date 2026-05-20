# Research Backlog

Outstanding RE and documentation tasks, grouped by blocker type. Only open items are listed — resolved items are recorded inline in the individual format docs.

## Requires Gameplay (differential save pass)

- **PLT text-region gaps (`0xB0`–`0xC1`, `0xCF`–`0x5AE`, `0x2018`–`0x20B7`, `0x21F8`–`0x25DF`)**: The numeric stats (kill tallies, mission counters, weapon accuracy at `0x1F80`–`0x21F7`) are fully mapped from RE. Remaining unknowns span four ranges: 18 bytes at `0xB0`–`0xC1` (possibly score level or rank index), 1,344 bytes at `0xCF`–`0x5AE` (between secondary string and mission log), 160 bytes at `0x2018`–`0x20B7` (between kill tallies and weapon accuracy blocks), and ~1,000 bytes at `0x21F8`–`0x25DF` (tail region — likely fort/campaign-phase stats and multiplayer scoring). RE confirmed: none of the four gap VA ranges have individual field accesses anywhere in the full decompile — only bulk block copies (`_SaveFile`/`_LoadFile_16` over the entire 0x25E0-byte struct). Differential save is genuinely the only path: vary rank/score/missions, compare saves at those ranges. See [formats/PLT.md](formats/PLT.md).

