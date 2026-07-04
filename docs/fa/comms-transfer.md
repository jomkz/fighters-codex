# CDRVXF32.DLL — Cdrv file-transfer comms driver

`CDRVXF32.DLL` is the **file-transfer driver**: a transfer UI (`FileTransferDialog`, `CdrvXfer*`) over a DOS-style file API (`dos_*`) and `cdrvxfer_*` send/receive. Imports the CDRVDL32 base.

The suite's shared design, third-party rationale, and the FA-side boundary are described in [comms.md](comms.md) (the CDRVDL32 base driver).

> **Provenance:** Ghidra static analysis of `CDRVXF32.DLL` (imported into `fa-re`, auto-analysed;
> public names from the PE export table). Third-party middleware, documented at the **boundary**
> ([#247](https://github.com/jomkz/fighters-codex/issues/247) /
> [#255](https://github.com/jomkz/fighters-codex/issues/255)): the exported ABI is named; internals
> and referenced data are waived, not reversed. Confidence per [spec-authoring.md](../spec-authoring.md).

![The Cdrv comms suite: FA.EXE's serial/modem multiplayer path drives CDRVDL32 (RS-232), CDRVHF32 (Hayes modem), CDRVXF32 (file transfer) and COMMSC32 (terminal), over the Win32 comms API.](diagrams/comms.svg)

## Functions

Representative exports (all named in the [symbol DB](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/comms-xf.csv)):

| VA | Export | Role |
|----|--------|------|
| `0x10002410` | `CdrvXferCreateDialog` | Cdrv export |
| `0x10002580` | `CdrvXferUpdateDialog` | Cdrv export |
| `0x100026A0` | `CdrvXferDestroyDialog` | Cdrv export |
| `0x10002A90` | `cdrvxfer_files` | Cdrv export |
| `0x10002AC0` | `cdrvxfer_sfiles` | Cdrv export |
| `0x10002AF0` | `FileTransferDialog` | Cdrv export |
| `0x10003080` | `cdrvxfer_gclose` | Cdrv export |
| `0x100033E0` | `cdrvxfer_getfiles` | Cdrv export |

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
