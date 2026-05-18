# Multiplayer Network Configuration (NET.DAT)

`NET.DAT` is a binary file that stores FA's multiplayer network settings.

## Location

Loose file in the FA install directory — not packed into any LIB archive.

## Observed Properties

| Property | Value |
|----------|-------|
| Filename | `NET.DAT` |
| Size     | 3,552 bytes (0xDE0) |
| Format   | Binary, mostly null-padded |

## Known Content

The file is 3,552 bytes and is predominantly null bytes with sparse non-null data. It likely stores IPX/TCP network addresses, player callsigns, session names, and modem settings — the full range of late-1990s multiplayer transport options FA supported.

## Structure Hypothesis

3,552 bytes (0xDE0), predominantly null-padded. The file likely stores one or more transport configuration blocks covering the late-1990s multiplayer options FA supported: IPX LAN, TCP/IP, and serial/modem.

Probable layout (hypothesis — verify against `CN_INFO` struct via Ghidra):

```
Offset     Size  Field
------     ----  -----
0x0000        4  magic or version tag
0x0004      128  player callsign (null-terminated)
0x0084       64  session/game name
0x00C4        1  transport type (0=IPX, 1=TCP/IP, 2=serial, 3=modem)
0x00C5      256  IP address or hostname (TCP/IP mode)
0x01C5      ...  modem init string / COM port settings
   ...      ...  (remainder null-padded)
```

## Differential Mapping

Since most of the file is null-padded, the quickest approach is:

1. Open FA multiplayer setup (even without a real network). Enter a callsign and session name.
2. Diff `NET.DAT` before and after — the callsign and session name will appear as ASCII strings; their offset is their field offset.
3. Change transport type (IPX → TCP/IP) and diff again; the transport byte will change.

## Ghidra Cross-Reference

FA.SMS contains `CN_INFO` (confirmed in the SMS symbol set). Load FA.EXE with `ImportFASms` labels:

1. Search for `CN_INFO` or `?CN_INFO@@` in the Symbol Table.
2. If it appears as a data label, its size and the fields accessed near it (via decompiler) will give the full struct layout without any live game testing.
3. Cross-reference with the `CN_ReadConfig` function that also reads `EA.CFG` — the two structs may share a common config block.

## TODO

- Read callsign and session name offsets via differential mapping
- Locate `CN_INFO` in Ghidra and map the full struct
- Confirm whether NET.DAT holds one transport block or a union of all transport configs

## Related

- [CFG.md](CFG.md) — general game configuration file
