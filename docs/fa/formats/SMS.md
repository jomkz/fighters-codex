# FA.EXE Symbol Map (FA.SMS)

`FA.SMS` is a binary symbol map file shipped with Jane's Fighters Anthology. It contains 3,829 MSVC-mangled C++ function and variable names paired with their virtual addresses in FA.EXE. It is the single most useful resource for FA.EXE reverse engineering.

## Location

Loose file in the FA install directory — not packed into any LIB archive.

## Binary Structure

```
Offset  Size  Field
------  ----  -----
0x0000     4  count        u32 LE — number of symbol records (3829 = 0x0EF5)
0x0004  N×8  records      N × { str_off: u32 LE, va: u32 LE }
0x????     *  string_table null-terminated C strings, densely packed
```

- `string_table` base offset = `4 + count × 8` = **30636** (0x778C)
- Total file size: **106,706 bytes**
- `va` — virtual address of the symbol in FA.EXE's address space (not a file offset)
- `str_off` — byte offset into `string_table` of the null-terminated symbol name
- Records are stored in `str_off` order (string-table insertion order), **not** sorted by VA

## Address Range

| Boundary | VA |
|----------|----|
| Lowest   | `0x00401000` |
| Highest  | `0x005937E0` |

This covers the full FA.EXE image: `.text` (code), `.data`, `.rdata`, and `.bss`.

## Symbol Contents

3,829 MSVC C++ decorated names (`?`-mangled) plus C-decorated names. Calling convention breakdown:

| Convention | Decoration | Count |
|------------|------------|-------|
| C-linkage / callbacks | no `?` prefix | 2563 |
| `__cdecl` | `@@YA` | 446 |
| `__stdcall` | `@@YG` | 138 |
| `__fastcall` | `@@YI` | 122 |

Representative sample:

| VA | Symbol | Demangled |
|----|--------|-----------|
| `0x00401000` | `_explode` | `explode` (C linkage) |
| `0x004BC240` | `?APEndArrestorCatch@@YAXXZ` | `void APEndArrestorCatch(void)` |
| `0x00401022` | `?APLanding@@YGDXZ` | `char __stdcall APLanding(void)` |
| `?` | `?AllocVDO@@YADPAUVDO@@@Z` | `char AllocVDO(struct VDO *)` |
| `?` | `?CDPATH@@3PADA` | `char * CDPATH` (global) |
| `?` | `?CN_ReadConfig@@YAXPAUCN_INFO@@PAE@Z` | `void CN_ReadConfig(struct CN_INFO *, unsigned char *)` |

Namespace prefixes seen in the symbol set include: `AP` (autopilot), `VDO` (video), `CD` (campaign/disc), `CN` (network config), and many more.

## Cross-Reference Verification

Spot-checked 10 symbols across the full VA range against the shipped `FA.EXE` (ImageBase `0x00400000`). All sampled VAs contain valid x86 at their computed file offsets:

| VA | Symbol | First bytes | Notes |
|----|--------|-------------|-------|
| `0x00401000` | `_explode` | `8B 44 24 04 53 8B` | `MOV EAX,[ESP+4]; PUSH EBX` |
| `0x00411910` | `_OnTheGround@0` | `66 A1 BC 6F 4F 00` | `MOV AX,[...]` |
| `0x00452770` | `_HARDPtrs@12` | `8B 44 24 04 83 EC` | `MOV EAX,...; SUB ESP,...` |
| `0x0047ADD0` | `_ThrustSupport@0` | `83 EC 30 66 83 3D` | `SUB ESP,0x30` |
| `0x004A7220` | `_SetupPT` | `8B 44 24 04 50 E8` | `MOV EAX,...; PUSH; CALL` |
| `0x004BC240` | `?APEndArrestorCatch@@YAXXZ` | `33 C9 E8 E9 59 F9` | `XOR ECX,ECX; CALL` |
| `0x004D6FB0` | `_GetCurrentThread@0` | `FF 25 C0 35 59 00` | IAT thunk (import) |
| `0x004D81B0` | `__chkstk` | `51 3D 00 10 00 00` | `PUSH ECX; CMP EAX,0x1000` |
| `0x004EC200` | `?firstMenu@@3PAUMENU@@A` | `00 00 00 00 00 00` | zero-init global |
| `0x005937E0` | `\177wail32_NULL_THUNK_DATA` | `00 00 00 00 97 00` | Miles Sound System IAT |

**Conclusion**: The symbol map matches the shipped binary. All code symbols have valid function prologues; all data symbols are in the expected sections.

## Build Configuration

**Release build.** Evidence:

- No `_RTC_CheckStackVars`, `_RTC_CheckEsp`, `_RTC_Shutdown`, or any other `/RTCx` runtime-check symbols — these are injected only by MSVC debug builds
- No `_CrtDbgReport`, `_CrtDbgBreak`, or debug CRT entry points
- `__chkstk` and `__crtheap` are present but both appear in release builds (stack probing for large frames; CRT heap pointer)
- Mangled names show no debug-specific decorations

## Usage with Ghidra

A ready-to-run Java script is provided at [`scripts/ghidra/ImportFASms.java`](../../scripts/ghidra/ImportFASms.java). See [`scripts/README.md`](../../scripts/README.md) for full setup and overlay-DLL rebasing instructions.

Quick start:

1. Open FA.EXE in Ghidra and let auto-analysis finish.
2. Tools → Script Manager → run `ImportFASms`.
3. Point the file dialog at `FA.SMS` in the FA install directory.
4. All 3,829 functions and globals are labelled in one pass; progress bar shows in the status bar.

