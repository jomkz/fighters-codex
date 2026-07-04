# FA.EXE Input — Joystick & Mouse

Player input device handling: the Win32 multimedia **joystick** API layer and the **mouse**
event ring. (Serial-cable and modem link transport, historically "input" territory, is
documented with the [network](network.md) transport layer per the reconstruction program's
ownership split.) Re-carved from a very broad nominal range into the two true device
clusters.

> **Provenance:** Ghidra static analysis of FA.EXE with [FA.SMS](formats/SMS.md) symbols applied; recorded in the [symbol database](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/input.csv) and applied to the Ghidra project. Progress: [reconstruction matrix](reconstruction.md). Markers follow [spec-authoring.md](../spec-authoring.md): confirmed · inferred · unknown.

## Devices

- **Joystick** (`0x494270–0x494B50`): a thin layer over the Win32 MM joystick API
  (`joyGetNumDevs`/`joyGetPos(Ex)`/`joyGetDevCapsA`) — **not** DirectInput. `ReadSticksRaw`
  polls axes, `InitJoysticks` enumerates, `GetJoystickType` maps to a configured stick type.
- **Mouse** (`0x499CF0–0x499E5B`): a WndProc hook feeding a 16-entry event ring the shell and
  cockpit poll.

![Input: the joystick MM-API poll and the mouse event ring feed control state each frame.](diagrams/input.svg)

## Functions

Full record: [`db/symbols/input.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/input.csv).

| VA | Symbol | Role |
|----|--------|------|
| `0x494270` | `ReadSticksRaw` | poll raw joystick axes (Win32 MM) |
| `0x4942D0` | `InitJoysticks` | enumerate/initialise joystick devices |
| `0x494430` | `GetJoystickType` | map a device to the configured stick type |
| `0x4944A0` | `ReadDevice` | read one device's current state |

## Open Questions

### 1. Calibration / mapping table

The axis calibration and control-mapping tables the poll path reads are named at their base;
their exact record layout would let the `.CFG` control bindings be edited from the doc.

*Status: open — re-static.*

## Related

- [network.md](network.md) — the serial/modem link transport (SER_/MOD_).
- [physics.md](physics.md) — control inputs feed the flight model.
- [hud.md](hud.md) — slew/padlock controls drive HUD symbology.
