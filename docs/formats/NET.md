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

## TODO — Deep Dive

- Hex-analyze before and after configuring multiplayer options to identify field offsets
- Search FA.SMS for network-related symbols (prefix `CN_` observed: `CN_ReadConfig`, `CN_INFO` struct) to locate the config struct definition in FA.EXE
- Determine whether the file stores one transport config block or multiple (one per transport type)

## Related

- [CFG.md](CFG.md) — general game configuration file
