# Research Backlog

Outstanding RE and documentation tasks, grouped by blocker type. Only open items are listed — resolved items are recorded inline in the individual format docs.

## Requires Gameplay (differential save pass)

- **PLT text-region gaps (`0xB0`–`0xC1`, `0xCF`–`0x5AE`, `0x2018`–`0x20B7`, `0x21F8`–`0x25DF`)**: The numeric stats (kill tallies, mission counters, weapon accuracy at `0x1F80`–`0x21F7`) are fully mapped from RE. Remaining unknowns span four ranges: 18 bytes at `0xB0`–`0xC1` (possibly score level or rank index), 1,344 bytes at `0xCF`–`0x5AE` (between secondary string and mission log), 160 bytes at `0x2018`–`0x20B7` (between kill tallies and weapon accuracy blocks), and ~1,000 bytes at `0x21F8`–`0x25DF` (tail region — likely fort/campaign-phase stats and multiplayer scoring). RE confirmed: none of the four gap VA ranges have individual field accesses anywhere in the full decompile — only bulk block copies (`_SaveFile`/`_LoadFile_16` over the entire 0x25E0-byte struct). Differential save is genuinely the only path: vary rank/score/missions, compare saves at those ranges. See [formats/PLT.md](formats/PLT.md).

- **CN_INFO `[0xc0]`–`[0x8e3]` unknown region (~2,180 bytes)**: Ghidra GUI xref analysis (2026-05-20) exhausted the full SPX/SAP vtable chain — all five SPX functions and three SAP functions confirmed; none access this range. `CN_SetFactoryDefaults` zeroes it entirely. The earlier "IPX sub-block" label was incorrect — the actual IPX-specific fields are at `[0xdc6]`–`[0xdcf]` (local SPX address) and `[0xdd0]`–`[0xdda]` (direct-connect address). Range is likely modem phone-book strings or NetBEUI session data; no `MOD_*`/`SER_*` function reads of this range have been confirmed. *Approach: differential save — vary modem config / phone entries, compare NET.DAT bytes in this range.* See [formats/NET.md](formats/NET.md).

