# CDRVHF32.DLL — Cdrv Hayes-modem comms driver

`CDRVHF32.DLL` is the **Hayes-modem driver**: dialling and modem control (`Dial`, `Modem*`), framed data streams (`DataStreamGet*`), and CRC (`CdrvCrc16`/`CdrvCrc32`). Imports the CDRVDL32 base.

The suite's shared design, third-party rationale, and the FA-side boundary are described in [comms.md](comms.md) (the CDRVDL32 base driver).

> **Provenance:** Ghidra static analysis of `CDRVHF32.DLL` (imported into `fa-re`, auto-analysed;
> public names from the PE export table). Third-party middleware, documented at the **boundary**
> ([#247](https://github.com/jomkz/fighters-codex/issues/247) /
> [#255](https://github.com/jomkz/fighters-codex/issues/255)): the exported ABI is named; internals
> and referenced data are waived, not reversed. Confidence per [spec-authoring.md](../spec-authoring.md).

![The Cdrv comms suite: the game executable's serial/modem multiplayer path drives CDRVDL32 (RS-232), CDRVHF32 (Hayes modem), CDRVXF32 (file transfer) and COMMSC32 (terminal), over the Win32 comms API.](diagrams/comms.svg)

## Functions

Representative exports (all named in the [symbol DB](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/comms-hf.csv)):

| VA | Export | Role |
|----|--------|------|
| `0x10001000` | `InitializePort` | Cdrv export |
| `0x100013D0` | `SetBaud` | Cdrv export |
| `0x10001430` | `SetFlowControlCharacters` | Cdrv export |
| `0x100014B0` | `SetFlowControlThreshold` | Cdrv export |
| `0x10001510` | `SetPortCharacteristics` | Cdrv export |
| `0x100015A0` | `UnInitializePort` | Cdrv export |
| `0x10001640` | `SetSpecialBehavior` | Cdrv export |
| `0x10001720` | `Dial` | Cdrv export |

## Open Questions

### 1. Internals

The waived internals are the third-party Cdrv implementation and statically-linked CRT —
deliberately not reversed (licensed middleware; the exported ABI above is the documented
boundary). No FA understanding depends on them.

*Status: resolved — boundary-documented (third-party; internals out of scope by license).*

## Related

- [network.md](network.md) — the game executable's multiplayer, which drives the serial/modem path.
- [comms.md](comms.md) — the CDRVDL32 base driver and suite overview.
- [reconstruction.md](reconstruction.md) — the program this binary belongs to.
