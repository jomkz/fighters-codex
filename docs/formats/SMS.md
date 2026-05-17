# FA.EXE Symbol Map (FA.SMS)

`FA.SMS` is a binary symbol map file shipped with Jane's Fighters Anthology. It contains 3,829 MSVC-mangled C++ function and variable names paired with their virtual addresses in FA.EXE. It is the single most useful resource for FA.EXE reverse engineering.

## Location

Loose file in the FA install directory — not packed into any LIB archive.

## Binary Structure

```
Offset  Size  Field
------  ----  -----
0x0000     4  count        u32 LE — number of symbol records (3829 = 0x0EF5)
0x0004  N×8  records      N × { va: u32 LE, str_off: u32 LE }
0x????     *  string_table null-terminated C strings, densely packed
```

- `string_table` base offset = `4 + count × 8` = **30636** (0x778C)
- Total file size: **106,706 bytes**
- `va` — virtual address of the symbol in FA.EXE's address space (not a file offset)
- `str_off` — byte offset into `string_table` of the null-terminated symbol name

## Address Range

| Boundary | VA |
|----------|----|
| Lowest   | `0x00401000` |
| Highest  | `0x005937E0` |

This covers the full FA.EXE image: `.text` (code), `.data`, `.rdata`, and `.bss`.

## Symbol Contents

3,829 MSVC C++ decorated names (`?`-mangled). Representative sample:

| VA | Symbol | Demangled |
|----|--------|-----------|
| `0x00401000` | `?APEndArrestorCatch@@YAXXZ` | `void APEndArrestorCatch(void)` |
| `0x00401022` | `?APLanding@@YGDXZ` | `char APLanding(void)` |
| `?` | `?AllocVDO@@YADPAUVDO@@@Z` | `char AllocVDO(struct VDO *)` |
| `?` | `?CDPATH@@3PADA` | `char * CDPATH` (global) |
| `?` | `?CN_ReadConfig@@YAXPAUCN_INFO@@PAE@Z` | `void CN_ReadConfig(struct CN_INFO *, unsigned char *)` |

Namespace prefixes seen in the symbol set include: `AP` (autopilot), `VDO` (video), `CD` (campaign/disc), `CN` (network config), and many more.

## Usage with Ghidra / IDA

Import as a symbol / label script:

1. Parse the binary: read `count`, iterate records, resolve each `str_off` into the string table.
2. For each record, call `createLabel(toAddr(va), demangle(name), true)` (Ghidra) or `set_name(va, name)` (IDA).
3. All 3,829 functions and globals will be named automatically, dramatically reducing anonymous stub count.

A loader utility (`ft sms load` or similar) could automate this for Ghidra via its scripting API.

## TODO

- Write a `ft sms` subcommand that dumps the symbol table to CSV or imports it into a Ghidra project
- Cross-reference selected VAs against FA.EXE to confirm the symbol map matches the shipped binary version
- Identify which build configuration / PDB this map was generated from (check for debug vs. release indicators in the mangled names)
