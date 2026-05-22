# Multiplayer Network Configuration (NET.DAT)

`NET.DAT`, `MODEM.DAT`, and `SERIAL.DAT` are binary files that store FA's multiplayer network
settings. All three share the same 3,552-byte `CN_INFO` format and are read/written by the same
`CN_ReadConfig`/`CN_WriteConfig` functions.

## Location

Three loose files in the FA install directory — none packed into any LIB archive.

| File | Purpose |
|------|---------|
| `NET.DAT` | Active transport selection; IPX/TCP/IP addresses and direct-connect info |
| `MODEM.DAT` | Modem phone book (8 player name + phone number pairs) and COM port selection |
| `SERIAL.DAT` | Serial (RS-232) COM port and baud rate preferences |

## Observed Properties

| Property | Value |
|----------|-------|
| Format   | Binary: 4-byte checksum + 3,548-byte `CN_INFO` struct (all three files) |
| Size     | 3,552 bytes (0xDE0) each |
| Encoding | Mostly null-padded; strings are null-terminated |

## Known Content

Each file is 3,552 bytes (4-byte checksum + 3,548-byte `CN_INFO` struct). The struct stores all
transport configuration simultaneously (IPX, TCP/IP, serial, modem) in one unified layout — the
active transport is selected at runtime by field `[0x54]`.

**Session name is NOT stored in NET.DAT.** The multiplayer lobby session/game name comes from
IP.CFG (`/n=` flag), which is passed to IP.EXE at launch. NET.DAT only holds transport
configuration and the Janes.net online identity.

**Transport union**: All transport configs coexist in one `CN_INFO` struct. The active transport is
chosen at runtime by `[0x54]`; all sub-blocks are always persisted.

**Phone book in MODEM.DAT**: `RunModemConfigurationScreen` reads/writes MODEM.DAT separately from
NET.DAT. It manages 8 phone-book slots (player name + phone number pairs) stored in CN_INFO
`[0xd0]`–`[0x5cf]`. In NET.DAT these bytes are always zeroed.

## File Layout

All three files are written by `CN_WriteConfig` (0x47f930): a 4-byte checksum followed by the
3,548-byte `CN_INFO` struct (`fwrite(param_1, 0xddc, 1, file)`). The filename is passed as a
second parameter so the same function handles NET.DAT, MODEM.DAT, and SERIAL.DAT.

All file offsets below = CN_INFO struct offset + 4 (checksum header).

```
File off.  CN_INFO  Size  Field
---------  -------  ----  -----
0x0000        —       4   checksum (CfigChecksum over CN_INFO block; length = 0xddc/0xdd8/0xdb0 for v3/v2/v1)
0x0004      [0x0]     4   version dword — must be 3 (current); 2 and 1 are migrated on read
0x0008      [0x4]    80   callsign / Janes.net online name (null-terminated, 80-byte field)
                           — seeded from _janesOnlineName by CN_SetFactoryDefaults and CN_ReadConfig
                           — also passed directly as name to SER_ExchangeNames in serial mode
0x0058      [0x54]    4   transport type dword — selects active protocol:
                             2 = modem (set by RunModemConfigurationScreen; confirmed by MODEM.DAT diff)
                             3 = serial / RS-232 (SER_Initialize2_5 checks for == 3)
                             4 = TCP/IP (doConfigurationScreen checks for == 4)
                             other values used by NetSetProtocol for IPX/NetBEUI
0x0064      [0x60]    4   baud rate index dword (factory default: 10)
                             7=9600 · 8=19200 · 9=38400 · 10=57600 · 11=57600 · 12=28800 · 13=115200
                             (SER_Initialize4 switch; also used by MOD_InitPortAndModem)
0x0068      [0x64]    4   serial COM port index dword (factory default: 0 = COM1; range 0–3)
                             — read by SER_Initialize1; written by MOD_FindModemAndInit autodetect
0x006C      [0x68]   84   modem phone number / mode string (null-terminated, 84-byte field)
                             — dial mode (param_2==0): holds dial number, passed to _Dial_12
                             — listen mode (param_2≠0): RunModemConfigurationScreen writes "LISTEN"
0x00C0      [0xbc]    4   modem COM port index dword (factory default: 8 = autodetect)
                             0–7 = COM1–COM8 (explicit); 8 = autodetect (MOD_Initialize1 scans registry)
                             range valid: 0–8 checked by MOD_Initialize
0x00C4      [0xc0]  0x280 phone book player names: 8 × 0x50-byte null-terminated strings (slots 0–7)
                           — MODEM.DAT only; zeroed in NET.DAT; edited by RunModemConfigurationScreen
                           — slot n starts at CN_INFO[0xc0 + n×0x50]; stride confirmed by differential save
0x0344      [0x340] 0x280 phone book phone numbers: 8 × 0x50-byte null-terminated strings (slots 0–7)
                           — MODEM.DAT only; zeroed in NET.DAT; edited by RunModemConfigurationScreen
                           — slot n starts at CN_INFO[0x340 + n×0x50]; stride confirmed by differential save
0x05C4      [0x5c0] 0x324 (unused padding) — all-zero in every tested MODEM.DAT capture (0–8
                           phone-book entries, callsign set, transport set to modem, Call and
                           Answer modes both exercised). FA's modem config screen has no Advanced
                           Setup dialog; no UI path writes to this range. Confirmed unreachable.
0x08E8      [0x8e4]   8   IP address hex string — 8 ASCII hex chars, e.g. "c0a80101" for 192.168.1.1
                           (null-checkable; if [0x8e4]==0 then IP/MAC binary fields are zeroed)
0x08F0      [0x8ec]  13   MAC/IPX node hex string — 12 ASCII hex chars + null, e.g. "001122334455"
                   ~~~~  [0x8f9]–[0xdab]: ~1,203 bytes — unknown (possibly padding / modem init strings)
0x0DB0      [0xdac]   4   appIO callback function pointer — set by SER/MOD/NET_Initialize from CN_INFO;
                           used for status dialogs during connection setup
0x0DB4      [0xdb0]  ?    CN_INFO_TCP sub-block start (added in v2; initialized by NetSetFactoryTCP)
                           — first 12 bytes (3 dwords) zeroed/set via protocol vtable slot +0x66
0x0DCA      [0xdc6]   4   local IPX/SPX network number — written by spxinit via getsockname()
0x0DCE      [0xdca]   4   local IPX/SPX node address bytes 0–3 — written by spxinit via getsockname()
0x0DD2      [0xdce]   2   local IPX/SPX node address bytes 4–5 — written by spxinit via getsockname()
0x0DD4      [0xdd0]   4   remote address field A — dual-use:
                           TCP/IP context: IP address binary (4 bytes decoded from hex string at [0x8e4] via _atohb(…,8))
                           IPX context: direct-connect target IPX network number (user-entered in RunIPXOptionsDialog)
0x0DD8      [0xdd4]   4   remote address field B (first 4 bytes) — dual-use:
                           TCP/IP: MAC address bytes 0–3
                           IPX: direct-connect target IPX node address bytes 0–3
0x0DDC      [0xdd8]   2   remote address field B (last 2 bytes) — dual-use:
                           TCP/IP: MAC address bytes 4–5
                           IPX: direct-connect target IPX node address bytes 4–5
0x0DDE      [0xdda]   1   remote address validity/flag — dual-use:
                           TCP/IP: 1 = IP+MAC fields successfully decoded; 0 = invalid
                           IPX: 1 = direct-connect address manually entered by user; 0 = not set
0x0DDF      [0xddb]   1   (padding / unused; end of v3 struct)
```

### Version migration (CN_ReadConfig)

| File version | Action |
|---|---|
| v3, size 0xddc | Accepted as-is; `_janesOnlineName` overwrites `[0x4]` if set |
| v2, size 0xdd8 | Upgraded: version set to 3; TCP sub-block re-initialized; IP/MAC binary decoded from hex strings at `[0x8e4]`/`[0x8ec]` |
| v1, size 0xdb0 | Upgraded: same as v2 migration |
| corrupt / missing | Factory defaults applied via `CN_SetFactoryDefaults` |

## Confirmed Functions

| Function | VA | Lines | Role |
|----------|----|-------|------|
| `CN_ReadConfig` | 0x47f7a0 | 98888 | Reads config file (NET.DAT / MODEM.DAT / SERIAL.DAT); populates CN_INFO; applies `_janesOnlineName` at `[4]` |
| `CN_WriteConfig` | 0x47f930 | 99014 | Writes 4-byte checksum + 0xddc-byte CN_INFO to named config file (NET.DAT / MODEM.DAT / SERIAL.DAT) |
| `CN_SetFactoryDefaults` | 0x47f6d0 | 98785 | Zeroes CN_INFO; sets version=3; seeds `[4]` from `_janesOnlineName`; calls `NetSetFactoryTCP` |
| `CfigChecksum` | 0x47f740 | 98851 | Checksums CN_INFO; length driven by version dword: 0xddc/0xdd8/0xdb0 |
| `NetSetFactoryTCP` | 0x4b0700 | 139845 | Writes 3 zero dwords to TCP sub-block start via protocol vtable slot +0x66 |
| `SER_Initialize` | 0x44cb20 | 57979 | Serial connection setup; reads `[0x54]`, `[0x60]`, `[0x64]` |
| `SER_Initialize4` | 0x44c990 | 57902 | Maps `[0x60]` baud rate index → internal timing constant |
| `MOD_InitPortAndModem` | 0x49a7d0 | — | Opens COM port at `[0x64]`; sends modem AT attention + init; called by MOD_FindModemAndInit |
| `MOD_FindModemAndInit` | 0x49a850 | — | Scans `HKLM\System\CurrentControlSet\Services` for attached modem; writes autodetected COM port to `[0x64]`; calls MOD_InitPortAndModem |
| `MOD_FindModemAndInitPCMCIA` | 0x49a9b0 | — | Same as above but scans `HKLM\Enum\PCMCIA` for a PCMCIA modem |
| `MOD_Initialize1` | 0x49ad00 | — | Tries MOD_InitPortAndModem (`[0xbc]`), then FindModemAndInit, then FindModemAndInitPCMCIA |
| `MOD_DoConnect` | 0x49ad70 | — | Dials `[0x68]` phone number or waits for incoming call; decodes carrier speed → baud index |
| `MOD_InitializeAndConnect` | 0x49af30 | — | Calls MOD_Initialize1 then MOD_DoConnect |
| `MOD_Initialize` | 0x49aff0 | 118822 | Top-level modem setup; called by `MP_Initialize` (0x494bc0); reads `[0x68]`, `[0xbc]` |
| `MOD_Shutdown` | 0x49b0d0 | — | Hangs up modem and releases COM port |
| `RunSerialConfigurationScreen` | 0x49b1d0 | — | Serial config UI; reads/writes SERIAL.DAT; edits callsign `[0x4]`, COM port `[0x64]`, baud `[0x60]` |
| `RunModemAdvSetupDialog` | 0x49c260 | — | Advanced modem setup dialog (`CN_INFO*` param); symbol confirmed, not yet decompiled |
| `RunModemConfigurationScreen` | 0x49c780 | — | Modem config UI; reads/writes MODEM.DAT; edits callsign, phone book `[0xd0]`–`[0x5cf]`, COM port |
| `NET_Initialize` | 0x4b0830 | 140115 | TCP/IP connection setup; reads `[0x54]` for `NetSetProtocol`, `[0xdac]` for appIO |
| `NET_StartQuery` | 0x4b0940 | — | Starts SAP session scan; calls `proto_ptr+0x42` (sapopensocket), registers SAP callback at `0x4b22a0` |
| `spxinit` | 0x496f40 | — | Creates/binds SPX socket; writes local IPX address to `[0xdc6]`–`[0xdce]` via `getsockname` |
| `spxinit2` | 0x497000 | — | Protocol init stub; sets `NET_PROTOCOL+0xb = 1`; does not access CN_INFO |
| `spxbuildaddress` | 0x497290 | — | Builds `NET_ADDRESS` from local SPX address at `[0xdc6]`–`[0xdce]` |
| `spxlisten` | 0x497010 | — | Creates server-side SPX socket; binds to socket `0x87ec` |
| `spxconnect` | 0x497150 | — | Connects SPX socket to a `NET_ADDRESS` |
| `sapopensocket` | 0x4874c0 | — | Creates IPX SAP socket for session discovery; reads direct-connect address from `[0xdd0]`–`[0xdda]` |
| `FUN_004b22a0` | 0x4b22a0 | — | SAP response callback; dispatches received packets to `FUN_004b23f0` |
| `FUN_004b23f0` | 0x4b23f0 | — | SAP packet dispatcher; handles type 0x2 (session ad), 0x14 (player ad), 0x18 (disconnect) |
| `RunIPXOptionsDialog` | 0x493780 | — | IPX options UI; reads/writes direct-connect address at `[0xdd0]`–`[0xdda]` |

## NET_PROTOCOL Struct (`spx_proto`)

The global `?spx_proto@@3UNET_PROTOCOL@@A` at `0x00501408` is the SPX protocol implementation
selected by `NetSetProtocol` when the connection type is IPX/SPX. `_proto_ptr` points to it after
`FUN_004b21f0` selects the matching entry from the protocol list at `PTR_PTR_0050c6cc`.

The struct is a flat array of function pointers (C-style interface, not a C++ vtable):

| Offset | Value | Function |
|--------|-------|----------|
| `+0x04` | `0x0001` | Protocol type short (1 = SPX) |
| `+0x0a` | flag | Initialized flag — set to return value of `+0x3a` during protocol selection |
| `+0x0b` | flag | Init flag — set to 1 by `spxinit2` |
| `+0x3a` | `0x00496f40` | `spxinit` — creates/binds SPX socket, writes local IPX address |
| `+0x3e` | `0x00497000` | `spxinit2` — protocol init stub (called by `NET_Initialize`) |
| `+0x42` | `0x004874c0` | `sapopensocket` — creates SAP socket for session discovery (called by `NET_StartQuery`) |
| `+0x46` | `0x00487670` | Session connect function (called in SAP callback when a session is found) |
| `+0x4a` | — | Packet processor (vtable; called in SAP callback for multi-packet responses) |
| `+0x4e` | — | Post-query setup (called by `NET_StartQuery` after `sapopensocket`) |
| `+0x5a` | — | Address extractor (called in SAP callback before `NETIsAddrLocal` check) |

Source file (from error strings): `E:\atf95\multi\sap.cpp`, `E:\atf95\multi\ipx.cpp`

## Related

- [CFG.md](CFG.md) — general game configuration file
