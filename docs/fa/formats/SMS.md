---
format: SMS
name: Game-executable Symbol Map
extensions: [".SMS"]
category: system
endianness: little
spec:
  status: complete
codec:
  direction: read
  rationale: "FA.SMS is a debugger symbol map (address → mangled name) read for reconstruction; it has no game-consumed write direction — the engine never loads a codex-authored .SMS — so `fx sms dump` emits CSV and there is nothing to serialize back (round-trip decision, #101)"
  lib: [lib/src/sms.cpp]
  commands: [sms]
  tests: [tests/test_sms.cpp]
  fuzz: []
  fixtures:
    synthetic: false
    real_manifest: true
related: []
---

# SMS — Game-executable Symbol Map (.SMS)

`FA.SMS` is a binary symbol map file shipped with Jane's Fighters Anthology —
a loose file in the FA install directory, not packed into any LIB archive. It
contains 3,829 MSVC-mangled C++ function and variable names paired with their
virtual addresses in the game executable. It is the single most useful resource for the game executable
reverse engineering.

## Tools

### fx

```
fx sms dump <FA.SMS> [-o out.csv]    # symbol table → CSV (va, mangled, demangled)
```

## File Layout

All multi-byte integers are little-endian.

| Offset | Size | Type | Description |
|--------|------|------|-------------|
| `0x0000` | 4   | u32 | count — number of symbol records (3829 = 0x0EF5) |
| `0x0004` | N×8 |     | records: N × { str_off: u32, va: u32 } |
| —        | *   |     | string_table — null-terminated C strings, densely packed |

- `string_table` base offset = `4 + count × 8` = **30636** (0x778C)
- Total file size: **106,706 bytes**
- `va` — virtual address of the symbol in the game executable's address space (not a file offset)
- `str_off` — byte offset into `string_table` of the null-terminated symbol name
- Records are stored in `str_off` order (string-table insertion order),
  **not** sorted by VA

### Address Range

| Boundary | VA |
|----------|----|
| Lowest   | `0x00401000` |
| Highest  | `0x005937E0` |

This covers the full the game executable image: `.text` (code), `.data`, `.rdata`, and `.bss`.

### Symbol Contents

3,829 MSVC C++ decorated names (`?`-mangled) plus C-decorated names. Calling
convention breakdown:

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

Namespace prefixes seen in the symbol set include: `AP` (autopilot), `VDO`
(video), `CD` (campaign/disc), `CN` (network config), and many more.

## Engine Notes

### Cross-Reference Verification

Spot-checked 10 symbols across the full VA range against the shipped `FA.EXE`
(ImageBase `0x00400000`). All sampled VAs contain valid x86 at their computed
file offsets:

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

**Conclusion**: the symbol map matches the shipped binary. All code symbols
have valid function prologues; all data symbols are in the expected sections.

### Build Configuration

**Release build.** Evidence:

- No `_RTC_CheckStackVars`, `_RTC_CheckEsp`, `_RTC_Shutdown`, or any other
  `/RTCx` runtime-check symbols — these are injected only by MSVC debug builds
- No `_CrtDbgReport`, `_CrtDbgBreak`, or debug CRT entry points
- `__chkstk` and `__crtheap` are present but both appear in release builds
  (stack probing for large frames; CRT heap pointer)
- Mangled names show no debug-specific decorations

### Key Confirmed Symbols

A curated list of all 3,829 named symbols is maintained in
[docs/fa/symbols.md](../symbols.md). Selected high-value symbols confirmed
during the main Ghidra disassembly pass:

| VA | Symbol | Notes |
|----|--------|-------|
| `0x403700` | `?usnfmain@@YAXXZ` | Main game loop entry point |
| `0x41E8F0` | `IsBrentDLL` | Test whether a loaded DLL uses Phar Lap BRF format |
| `0x41EB60` | `LoadDLL` | Generic overlay DLL loader |
| `0x41F240` | `LoadBrentDLL` | Load a Phar Lap BRF overlay DLL |
| `0x441C60` | `_ChooseScoreInit` | Score / debrief screen initialiser |
| `0x464C80` | `_CTDo_*` range start | AI condition/action dispatcher — in the game executable itself (see AI.md) |
| `0x463EA0` | `_MaskEvents_4` | Entity flag bit 10 event-mask handler |
| `0x464040` | `_Reaction_12` | Entity flag bit 10 reaction handler |
| `0x467110` | `_CTEval_*` range end | AI condition evaluator — in the game executable itself |
| `0x467180` | `PilotSave(PILOT*, short)` | Write pilot data to `PLTnnn.P` save file |
| `0x480B50` | `_MISSIONInit2_0` | Mission system second-phase init |
| `0x481940` | `_CallMissionProc_8` | Dispatch per-mission condition proc |
| `0x481C10` | `_MISSIONTextProc@16` | MM text parser (tmap / tmap_named / tdic keywords) |
| `0x486860` | `_MISSIONCheckSuccess@0` | Check win/loss conditions |
| `0x4A6EB0` | `SetupOT` | OBJ_TYPE init (BRF type init entry) |
| `0x4A7230` | `SetupJT` | PROJ_TYPE init |
| `0x4AACF0` | `T_HorizonProc` | Horizon renderer — exported by the game executable, consumed by .LAY DLLs |
| `0x4B4320` | `WRFogLayerUpdate` | Per-frame fog opacity jitter — confirmed the game executable export |
| `0x4C5D70` | `@T_Load@4` | T2 terrain file loader entry point |
| `0x4D22D4` | `do_ifdestroyed` | Shape bytecode opcode handler — tests destroyed state |

### Usage with Ghidra

A ready-to-run Java script is provided at
[`scripts/ghidra/ImportFASms.java`](https://github.com/jomkz/fighters-codex/blob/main/scripts/ghidra/ImportFASms.java).
See [`scripts/ghidra/README.md`](https://github.com/jomkz/fighters-codex/blob/main/scripts/ghidra/README.md)
for full setup and overlay-DLL rebasing instructions.

Quick start:

1. Open the game executable in Ghidra and let auto-analysis finish.
2. Tools → Script Manager → run `ImportFASms`.
3. Point the file dialog at `FA.SMS` in the FA install directory.
4. All 3,829 functions and globals are labelled in one pass; progress bar
   shows in the status bar.

## Related

**Engine:** [symbols.md](../symbols.md) — the curated, subsystem-organized
symbol reference built from this file; every `docs/fa` engine doc cites VAs
recovered through it.
