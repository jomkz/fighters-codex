# MSAPI.DLL — matchmaking / internet-play client

`MSAPI.DLL` is the **matchmaking / internet-play client** the game executable links for
online multiplayer — EA's own *MServerDLL* (its `InternalName`), not the DirectPlay /
serial paths in [network.md](network.md). It is a thin Winsock client for an EA
**matchmaker server**: it logs a player or host in, lists and selects games, uploads and
downloads game/player records, and reports mission results, all over a single TCP
connection.

Unlike the third-party middleware in the install (WAIL32 = Miles, the Cdrv comms suite),
`MSAPI.DLL` is **EA-authored** — the PE version resource carries `CompanyName "Electronic
Arts"`, `LegalCopyright "Copyright (C) 1998, Electronic Arts"`, and `InternalName
"MServerDLL"` — so it gets a full reconstruction of its own code. It is built on MFC
(`CWinApp`/`CWinThread`/`CSocket`); that statically-linked MFC/MSVC-CRT framework is the
**boundary** and is waived, not reversed.

> **Provenance:** Ghidra static analysis of `MSAPI.DLL` (imported into `fa-re`,
> auto-analysed; public names from the PE export table, internals named by tracing the
> Winsock protocol). EA-authored: the 14 `*MS*` exports and the matchmaker protocol code are
> reversed; the statically-linked MFC / MSVC-CRT framework is waived at the boundary
> ([#272](https://github.com/jomkz/fighters-codex/issues/272) ·
> [#274](https://github.com/jomkz/fighters-codex/issues/274) ·
> [#275](https://github.com/jomkz/fighters-codex/issues/275)). Confidence per
> [spec-authoring.md](../spec-authoring.md).

![The matchmaker link: the game executable links six MSAPI exports; connectMS reads the Server IP/Port from the registry, opens a TCP socket and does the WAKEUP/OK handshake; every other export is a single-byte command with network-order length-prefixed payloads to the EA matchmaker server over Winsock.](diagrams/matchmaking.svg)

## The matchmaker link

**Transport.** `connectMS` resolves the server endpoint from the registry — a small
registry-cache helper class (`ms_reg_open` → `ms_reg_select_subkey` → `ms_reg_read_value`,
with get-or-create semantics) reads **`Server IP`** and **`Server Port`** from
`HKLM\SOFTWARE\…\Matchmaker` (or takes a default) — opens a TCP socket
(`socket(AF_INET, SOCK_STREAM)`, kept in `ms_socket`), `connect()`s, then sends the literal
string **`WAKEUP`** (6 bytes) and expects **`OK`**. From then on the socket is the single
duplex link for every command.

**Framing.** Each export sends a **single-byte ASCII command opcode**, followed (per
command) by network-byte-order **u32 length prefixes** and payloads, and reads a one- or
two-byte response ack (`O` / `OK` / `K`). Two primitive pairs do all the I/O:
`ms_send_all` / `ms_recv_all` loop `send()` / `recv()` until exactly *N* bytes have moved
(logging `"Send/Read Packet Error - Correcting..."` on a short transfer), and
`ms_send_u32` / `ms_recv_u32` wrap them with `htonl` / `ntohl` for the length prefixes.

**Session.** `initializeMS` performs the registration handshake — it sends the record size,
the `u` (upload record) command and the player record, then `ms_upload_init_arrays` (the
`i` command: two groups of three `htonl`-byteswapped, length-prefixed u32 arrays), then the
machine's volume serial number (`GetVolumeInformation`, formatted with `"%d"`) — and creates
the background receive worker `ms_recv_thread` (an MFC `CWinThread`, `AfxBeginThread` /
`CREATE_SUSPENDED`) which is resumed in host-login mode and suspended in player mode.

## The wire protocol

Every command is one lowercase ASCII byte (`getMSdatafile`'s mid-transfer ack is the
literal `O`):

| Opcode | Export | Payload → response |
|--------|--------|--------------------|
| `u` | `initializeMS` / `updateMSgame` | u32 size + player/game record → (upload) |
| `i` | `ms_upload_init_arrays` | 2×3 length-prefixed u32 arrays → (registration) |
| `h` | `loginMShost` | — → resume receive worker |
| `p` | `loginMSPlayer` | — → suspend receive worker |
| `r` | `requestMSgame` | — → stream of `P` records (u32-length-prefixed) |
| `s` / `d` | `selectMSgame` / `deselectMSgame` | u32 game id → `O` |
| `t` | `resetMSfilter` | — → reset list cursor |
| `f` | `fetchMSgame` | u32 game id → `P` + record |
| `v` | `sendMSresults` | u32 length + results blob → `O` |
| `z` | `getMSdatafilesize` | name → u32 size (`0xFFFFFFFF` = not found) + `O` |
| `x` | `getMSdatafile` | name → file bytes; client acks `O`, server replies `K` |
| `l` | `closeMS` | — → close socket |

`requestMSgame` accumulates the `P`-prefixed game records into a linked list of 0x24-byte
nodes (`ms_game_list_head` / `ms_game_list_tail` / `ms_game_selected`; each node is
`+0x10` length · `+0x14` payload pointer · `+0x1C` next · `+0x20` list head), guarded by a
critical section, and hands the caller a 5-dword header plus the record payload.

**Return codes.** `1` = OK; `0x3E8` = `socket()` failed; `0x3E9` = handshake rejected;
`0x3EA` = connect / socket error; `0x3EB` = protocol (short send/recv); `0x3EC` = not
connected; `0x3ED` = bad arguments; `0x3EE` = empty game list; `0x3EF` / `0x3F1` / `0x3F2` /
`0x3F3` = command-specific NAKs (select / results / datafile-size / datafile).

## Functions

The matchmaker exports and the protocol internals (all named in the
[symbol DB](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/msapi.csv)):

| VA | Function | Role |
|----|----------|------|
| `0x100011E0` | `connectMS` | registry endpoint → TCP socket + `WAKEUP`/`OK` handshake |
| `0x10001670` | `initializeMS` | registration handshake; create receive worker |
| `0x10001A20` | `ms_upload_init_arrays` | opcode `i` — upload the init u32 arrays |
| `0x10001D20` | `loginMShost` | opcode `h` — host login; resume worker |
| `0x10001D80` | `loginMSPlayer` | opcode `p` — player login; suspend worker |
| `0x10001DE0` | `requestMSgame` | opcode `r` — download the game list |
| `0x10002030` | `selectMSgame` | opcode `s` — select a game |
| `0x100020C0` | `deselectMSgame` | opcode `d` — deselect a game |
| `0x10002120` | `resetMSfilter` | opcode `t` — reset the list cursor |
| `0x10002170` | `updateMSgame` | opcode `u` — upload/update a record |
| `0x100021D0` | `fetchMSgame` | opcode `f` — fetch one game record |
| `0x10002280` | `sendMSresults` | opcode `v` — report mission results |
| `0x10002330` | `getMSdatafilesize` | opcode `z` — query server data-file size |
| `0x10002440` | `getMSdatafile` | opcode `x` — download a server data-file |
| `0x10002570` | `closeMS` | opcode `l` — quit / close socket |
| `0x10002630` | `ms_recv_all` | `recv()` exactly N bytes |
| `0x10002680` | `ms_send_all` | `send()` exactly N bytes |
| `0x100026D0` | `ms_recv_u32` | `recv` + `ntohl` length prefix |
| `0x10002700` | `ms_send_u32` | `htonl` + `send` length prefix |
| `0x10002730` | `ms_disconnect` | worker teardown: quit + close + free list |
| `0x10002800` | `ms_reg_open` | registry-cache: open the base keys |
| `0x10002950` | `ms_reg_close` | registry-cache: flush + close |
| `0x100029A0` | `ms_reg_select_subkey` | registry-cache: open a named subkey |
| `0x10002A00` | `ms_reg_read_value` | registry-cache: read a value (get-or-create) |
| `0x100034D0` | `ms_atoi` | decimal string → int (parses the port) |

## FA.EXE ↔ matchmaker boundary

The game executable links **six** of the fourteen exports — `initializeMS`, `connectMS`,
`sendMSresults`, `getMSdatafilesize`, `getMSdatafile`, `closeMS` (see
[external-imports.md](external-imports.md)) — i.e. the *connect → play → report results +
pull server data-files → disconnect* path. The remaining eight (`login*`, `requestMSgame`,
`select`/`deselect`/`fetch`/`updateMSgame`, `resetMSfilter`) form the game-browse / lobby
half of the API. FA.EXE holds the socket only inside `MSAPI.DLL`; it never touches Winsock
for matchmaking itself — the DLL is the whole boundary.

## Open Questions

### 1. On-wire record layouts and the matchmaker server

The field layouts of the player record (uploaded by `u` / `i`) and the `P` game records are
opaque from the client alone — they are sized by `ms_record_size` and moved as opaque
blobs; only a live matchmaker server (long defunct) or a packet capture would pin the
fields. The server itself is not part of the FA install.

*Status: open — re-gameplay / unrecoverable (the EA matchmaker service is defunct).*

### 2. MFC / MSVC-CRT framework internals

The bulk of `MSAPI.DLL` is statically-linked MFC (`CWinApp` / `CWinThread` / `CSocket`) and
MSVC-CRT — deliberately not reversed (framework code; the matchmaker protocol above is the
documented boundary). No FA understanding depends on it.

*Status: resolved — boundary-documented (third-party framework; internals out of scope).*

## Related

- [external-imports.md](external-imports.md) — the FA.EXE → `MSAPI.dll` import surface.
- [network.md](network.md) — the game executable's multiplayer, which drives `MSAPI.dll`.
- [ip-tool.md](ip-tool.md) — IP.EXE, the other EA-authored MFC companion binary.
- [reconstruction.md](reconstruction.md) — the program this binary belongs to.
