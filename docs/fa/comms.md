# CDRVDL32.DLL — Cdrv RS-232 serial comms driver

`CDRVDL32.DLL` is the **base RS-232 serial driver** of the suite: raw serial I/O (`ser_rs232_*`), port management, and the `bio_*` timer helpers. The modem, transfer, and terminal DLLs all import it.

## The Cdrv comms suite

FA.EXE's serial/modem multiplayer path — the peer of the DirectPlay / SPX/IPX path in [network.md](network.md) — is built on a four-DLL **Cdrv** comms library: **CDRVDL32** (this file, RS-232 base), **CDRVHF32** (Hayes modem), **CDRVXF32** (file transfer), and **COMMSC32** (terminal screen). It is generic serial/modem middleware (a `.commdrv` section, a complete `Cdrv*` / `ser_rs232_*` ABI, the Win32 comms API underneath, no game-specific content) — **licensed third-party code**, boundary-documented like WAIL32 (Miles) and the MS redistributables: the exported ABI is named, the internals are waived, not reversed.

> **Provenance:** Ghidra static analysis of `CDRVDL32.DLL` (imported into `fa-re`, auto-analysed;
> public names from the PE export table). Third-party middleware, documented at the **boundary**
> ([#247](https://github.com/jomkz/fighters-codex/issues/247) /
> [#255](https://github.com/jomkz/fighters-codex/issues/255)): the exported ABI is named; internals
> and referenced data are waived, not reversed. Confidence per [spec-authoring.md](../spec-authoring.md).

![The Cdrv comms suite: FA.EXE's serial/modem multiplayer path drives CDRVDL32 (RS-232), CDRVHF32 (Hayes modem), CDRVXF32 (file transfer) and COMMSC32 (terminal), over the Win32 comms API.](diagrams/comms.svg)

## Functions

Representative exports (all named in the [symbol DB](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/comms-dl.csv)):

| VA | Export | Role |
|----|--------|------|
| `0x100019D0` | `ser_rs232_block` | Cdrv export |
| `0x100019E0` | `ser_rs232_cleanup` | Cdrv export |
| `0x10001A90` | `ser_rs232_dtr_off` | Cdrv export |
| `0x10001AE0` | `ser_rs232_dtr_on` | Cdrv export |
| `0x10001B30` | `ser_rs232_flush` | Cdrv export |
| `0x10001C50` | `ser_rs232_getbyte` | Cdrv export |
| `0x10001D20` | `ser_rs232_getpacket` | Cdrv export |
| `0x10001DF0` | `ser_rs232_getport` | Cdrv export |

## Open Questions

### 1. Internals

The waived internals are the third-party Cdrv implementation and statically-linked CRT —
deliberately not reversed (licensed middleware; the exported ABI above is the documented
boundary). No FA understanding depends on them.

*Status: resolved — boundary-documented (third-party; internals out of scope by license).*

## Related

- [network.md](network.md) — FA.EXE's multiplayer, which drives the serial/modem path.
- [comms.md](comms.md) — the CDRVDL32 base driver and suite overview.
- [reconstruction.md](reconstruction.md) — the program this binary belongs to.
