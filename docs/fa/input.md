# Input — Keyboard, Joystick & Mouse

Player input: the **keyboard model** (the Windows-message hook, the scan→code translation
table, and `FlightKey` — the in-flight command table), the **control-device layer** (the
`deviceProcs` protocol that lets the stick/rudder/throttle each be keyboard or joystick),
the Win32 multimedia **joystick** API layer, and the **mouse** event ring. (Serial-cable
and modem link transport, historically "input" territory, is documented with the
[network](network.md) transport layer per the reconstruction program's ownership split.)

> **Provenance:** Ghidra static analysis of the game executable with [FA.SMS](formats/SMS.md) symbols applied; recorded in the [symbol database](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/input.csv) and applied to the Ghidra project. Progress: [reconstruction matrix](reconstruction.md). Markers follow [spec-authoring.md](../spec-authoring.md): confirmed · inferred · unknown.

## The keyboard model (#492)

`KEYEvent` (`0x411600`) is the `WM_KEYDOWN`/`WM_SYSKEYDOWN`/`-UP` hook. Under a critical
section it maintains, per event: confirmed

- **Qualifier state** — shift scans (`0x2A`/`0x36`) set `_qualStatus` bit 0, Ctrl (`0x1D`)
  bit 1, Alt (`0x38`) bit 2 (with a parallel left/right mask in `_keyFlags`).
- **Held-key state** — `_keyarray[256]` (`0x522598`), indexed by scan code, set 1 on down /
  0 on up. The axis and fire keys are *polled* from this array each frame (see the device
  procs), so `KEYEvent` drops auto-repeat events (bit 30 of `lParam`) for the scans in the
  `_ignoreRepeats` list — keypad 4/6/8/2/5/1/3, keypad +/−, `=`, `-`, Tab, Space — while
  in flight (`_curScreen == 0x10`).
- **The key-code queue** — a 16-entry ring (`_kbBuffer`, head/tail/count) of 16-bit key
  codes, drained by `_GetKey` (with `GetFakeKey` as a second, synthetic source that the UI
  and the joystick buttons inject into via `PutFakeKey`).

**Key codes.** The translation table `__kbScanToASCII` (`0x4ECC00`, 256 × u16, indexed
`scan | (shift ? 0x80 : 0)`) defines the code space: confirmed

| key class | table entry | resulting code |
|---|---|---|
| printable + control keys | ASCII in the **high** byte | **bare ASCII** — `'a'` = `0x61`, Shift+1 = `'!'` = `0x21`, Esc = `0x1B`, Enter = `0x0D`, Backspace = `0x08`, Tab = `0x09`, Space = `0x20` |
| keypad / nav keys (incl. shifted and `E0`-extended) | scan in the **low** byte | normalized **`scan << 8`** — Up = `0x4800`, PgDn = `0x5100`, Ins = `0x5200` … |
| F-keys, or any key with Ctrl/Alt held | none / not translated | **`scan << 8 \| qual`** — F1 = `0x3B00`, F12 = `0x5800`, Ctrl+A = `0x1E02`, Alt+E = `0x1204`, Alt+F4 = `0x3E04` |

Every key consumer in the shell and the sim — `FlightKey`, `SlewKey`, `INFO2Screen`,
`FlightMenu`, the dialogs — dispatches on exactly this space.

## The control-device layer

`_deviceProcs` (`0x4EE310`) is an 11-entry function table: index 0 = `KeyStick`, 1–6 =
`PotStick` (the six configured joystick models), 7 = `KeyRudder`, 8 = `PotRudder`, 9 =
`KeyThrottle`, 10 = `PotThrottle`. `_stickDevice`/`_rudderDevice`/`_throttleDevice` select
one proc each (the in-flight menu's Control menu writes them). Every proc implements a
4-message protocol: confirmed

| msg | meaning | notes |
|---|---|---|
| 0 | init | `InitPlayerControl` sends it to all three axes |
| 1 | calibrate | `InputCalibrate`; `PotThrottle` runs the "move throttle to 100%" dialog and stores the military-power detent (`_throttle100`) |
| 2 | poll | `GetPlayerControl` each frame, after `ReadSticksRaw` |
| 3 | key event | offered the key code by `FlightKey` **before** its own dispatch; a proc may consume (return 0) or translate |

- **`KeyStick`** polls `_keyarray` directly: arrows = full deflection (±`0x100`; Shift+
  arrows are left for the view code, Alt+arrows for thrust vectoring), and — regardless of
  the configured stick — latches **Space → `_weaponButton`** and **Tab → `_gunButton`**
  (`GetPlayerControl` always calls `KeyStick(2,0)` for exactly this). confirmed
- **`KeyThrottle`** consumes keys `'1'`–`'8'` (msg 3): presets 0/25/50/75/100 %, `'6'` =
  **101 = afterburner**, `'7'`/`'8'` = RPM −5/+5. confirmed
- **`KeyRudder`** polls keypad 1 / keypad 3 for full left/right rudder. confirmed
- **`PotStick`** maps the joystick buttons by stick model (gun, weapon, and a third button
  that injects key `0x0D` — the target-select command), routes the POV hat to view slew
  (`_coolieSlew`) or thrust vector (`_coolieTV`, `gameMultiPrefs` bit `0x800`), and holds
  Shift to convert the stick into a view control with buttons 1/2 zooming. confirmed
- **`PotThrottle`** maps the calibrated range to 0–100 with a hysteresis of 3 and an
  **afterburner detent** past the calibrated 100 % point. confirmed

`InitPlayerControl` also loads `vis240.SEE` into the 57-byte view/visibility block at
`0x4EE348` that `ServicePlayer` hands to `PROJInFOV` for its is-the-target-visible check.
confirmed

## FlightKey — the in-flight command table

`FlightKey` (`0x414690`, 5,724 B) is the command dispatch for a live plane player: it first
offers the key to the three device procs (msg 3), then executes one of ~70 commands.
The key legends below follow the US layout through the translation table; the dispatch
targets are read from the binary. confirmed

**Flight controls**

| key | code | action |
|---|---|---|
| `a` | `0x61` | autopilot toggle (`SetAutopilot`; envelope/fuel/damage checks first) |
| `b` | `0x62` | brakes toggle (`FMBrakes`, plane-flags bit `0x80`) |
| `f` | `0x66` | flaps toggle (`FMFlaps`, bit `0x100`) |
| `g` | `0x67` | gear toggle (`FMGear`, bit `0x40`) |
| `h` | `0x68` | hook toggle (`FMHook`, bit `0x400`) |
| `o` | `0x6F` | bay doors toggle (`FMBay`, bit `0x200`) |
| `1`–`8` | `0x31`–`0x38` | throttle 0/25/50/75/100 %, afterburner, RPM −5/+5 (consumed by `KeyThrottle`) |
| `z` / `x` | `0x7A`/`0x78` | thrust-vector nozzles +10 / −10 (`FMVector`) |
| `Z` / `X` | `0x5A`/`0x58` | nozzles aft (`FMVector(0,0)`) / reverse thrust (−90 or −180 by RPM) |
| `0` | `0x30` | thrust vector reset (`FMSetTV(0,0)`) |
| Ctrl+keypad 8/2/4/6 | `0x4802` … | `FMSetTV` nozzle presets (planes with the TV type flags) |
| `Shift+E` ×2 | `0x45` | **eject** (needs the same key twice; score −1/−2, AI takes the plane, view switch, MP notify) |

**Sensors & cockpit**

| key | code | action |
|---|---|---|
| `r` | `0x72` | air-to-air radar on/off |
| `R`, Ctrl+R | `0x52`, `0x1302` | air-to-ground radar on/off |
| `i` | `0x69` | IR seeker on/off |
| `m` | `0x6D` | HARM seeker on/off |
| `j` | `0x6A` | jammers on/off (`HARDFindJammer` types 2+3) |
| `y` | `0x79` | radar history trails on/off |
| `,` / `.` | `0x2C`/`0x2E` | radar range up / down (`CPRadarRange`) |
| `"` / `:` | `0x22`/`0x3A` | bomb-scope range up / down (`CPBombRange`) |
| `A` / `G` | `0x41`/`0x47` | AWACS / GCI support-radar view on/off (`_onSuppRadar`, `SAYSuppRadarMessage`) |
| `n` | `0x6E` | HUD on/off (`_hud`) |
| `{` / `}` | `0x7B`/`0x7D` | HUD brightness down / up |
| `u` | `0x75` | IFF squawk (`HUDSquawk`) |
| `v` | `0x76` | copy cockpit view (`CPCopyView`) |
| Shift+1…0 | `0x21`… | toggle cockpit MFD window 1…10 (`CPToggleWindow`; Shift+2 = window 11 with realistic avionics) |
| `d` | `0x64` | damage report (`DAMAGEReport`) |

**Targeting**

| key | code | action |
|---|---|---|
| Enter (or joystick btn 3) | `0x0D` | select next visible target (raster order) |
| `'` | `0x27` | select the target nearest the boresight |
| `t` / `T` | `0x74`/`0x54` | next / previous radar target (`CPNextTarget`) |
| `;` | `0x3B` | drop target |
| `/` , `\` | `0x2F`, `0x5C` | next / boresight radar **contact** (realistic-avionics pref) |
| Ctrl+A | `0x1E02` | padlock nearest threat (`HUDFindNearest(0xC000,0)`) |
| Ctrl+Z / Ctrl+X | `0x2C02`/`0x2D02` | `HUDFindNearest` variants (mask `0xC000`/`0x1E00`, flag `0x80`) |
| `w` / `W` | `0x77`/`0x57` | next / previous waypoint (`WPChange`) |

**Weapons**

| key | code | action |
|---|---|---|
| Space | polled | fire selected weapon (`_weaponButton` → `PROJFire`; bay-door and refire-interval checks) |
| Tab | polled | fire gun (`_gunButton`) |
| `[` / `]` | `0x5B`/`0x5D` | previous / next weapon (`HARDFindProj`) |
| keypad Ins / Del | `0x5200`/`0x5300` | launch chaff / flare (`PROJLaunchDevice`, "%d left" HUD message) |
| `J` / `K` | `0x4A`/`0x4B` | jettison drop tanks (store class 8) / ordnance (class 7) |

**Wingman & radio** (`MSGSend` command messages to the wing; legends inferred, payloads confirmed)

| key | code | action |
|---|---|---|
| Alt+E / Alt+R | `0x1204`/`0x1304` | wingman / wing: engage my target (cmd `0xB`, hostile-check) |
| Alt+F | `0x2104` | wingman: engage my radar contact |
| Alt+W | `0x1104` | attack my target's group (cmd `0xC` with target type word) |
| Alt+P | `0x1904` | engage my attacker (cmd `0xB`, `curId \| 0x60000000`) |
| Alt+D | `0x2004` | disengage / rejoin (cmd `0xB`, target 0) |
| Alt+T | `0x1404` | cycle formation type (`WNGSetType (n+1) % 3`) |
| Alt+H / Alt+V | `0x2304`/`0x2F04` | toggle horizontal (0x200↔0x800) / cycle vertical (0, +0x200, −0x200) spacing |
| Alt+C | `0x2E04` | toggle wing control mode (`WNGSetControl`) |
| Alt+B | `0x3004` | wing broadcast (cmd `0x10`) |
| Alt+1 … Alt+9 | `0x204`…`0xA04` | quick directives: cmd 5/6 with signed turn payloads (±170°, ±70° vertical, ±45°, ±35° as 1/65536-circle words) |
| Alt+S | `0x1F04` | radio silence toggle ("Radio traffic OK"/"Radio silence") |
| `F` / `I` | `0x46`/`0x49` | fort-mission status / aircraft report (multiplayer) |
| Ctrl+V | `0x2F02` | score overlay toggle (`_valkyriesScore`) |
| Backspace | `0x08` | `gamePrefs ^= 0x4000` — unlabeled toggle unknown |

## Slew mode

When a slew object is active (`_slewId`), `SlewKey` (`0x413D10`) runs first: keypad 8/2 =
fore/aft, 4/6 = left/right (rotated into the object heading), 9/3 = altitude up/down,
Ctrl+keypad 7/9 = heading ±2°, keypad 0 = speed ×2, keypad `.` = speed ÷2 (min `0x100`),
Ctrl+S = cycle to the next slewable object (object flag bit 0), Esc = exit slew. Consumed
keys return 0. confirmed

## ServicePlayer — the per-frame player service

`ServicePlayer` (`0x4164B0`, 3 KB) runs the player once per frame: stick decay under time
compression, `DAMAGEUpdate`, dropping an out-of-view target (pref `0x10000000` +
`PROJInFOV`), autopilot maintenance (auto-disengage out of fuel/envelope), the
`FMFlight()` tick with its crash dispatch (`PLANECrash`, "You have crash landed", per-system
damage bumps), **arrestor-hook catch** (a `Collision` probe of type `0x84` against the
airport object → `_HOOK_5K` sound, landing-quality grade from stall-speed and pitch
margins, state `0x16` + forced autopilot), waypoint auto-advance (0x44-stride list, passed
flag bit `0x80`), the collision → `MSGSend(0x4000)` + `Kill` path, afterburner/engine
sound triggers (previous-frame plane-flags copy at `0x50CFF3`), and the trigger logic —
`_weaponButton`/`_gunButton` edges → `PROJFire` with bay-door, refire-interval
(store `+0xF3`), and radar-guided-target selection. confirmed

Its helpers drive the on-ground state machine: `PlayerUpdateState` (taxi/catapult/takeoff/
approach/parking states; the carrier catapult is `APTakeoffType == 7` → state 7 + forced
autopilot; the `~BGUN.PT` fort-gun player skips all of it), `PlayerAutoRearm` +
`PlayerNeedsRearm` (auto rearm/refuel when stopped at a friendly airport), and
`PlayerPickParking` (nearest of the airport's four parking points → states `0x1B`–`0x1E`).
confirmed

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
| `0x411600` | `KEYEvent` | the Windows keyboard hook: qualifiers, `_keyarray`, the code ring |
| `0x414690` | `FlightKey` | the in-flight command table (above) |
| `0x413D10` | `SlewKey` | slew-mode keypad commands |
| `0x4164B0` | `ServicePlayer` | the per-frame player service |
| `0x417150` | `PlayerUpdateState` | ground/air state machine (taxi → catapult → … → parking) |
| `0x4178D0` | `KeyStick` | keyboard stick device proc (arrows; Space/Tab fire buttons) |
| `0x417C20` | `KeyThrottle` | throttle presets `'1'`–`'8'` |
| `0x417760` | `InitPlayerControl` | device init + `vis240.SEE` view config |
| `0x494270` | `ReadSticksRaw` | poll raw joystick axes (Win32 MM) |
| `0x4942D0` | `InitJoysticks` | enumerate/initialise joystick devices |
| `0x494430` | `GetJoystickType` | map a device to the configured stick type |
| `0x4944A0` | `ReadDevice` | read one device's current state |

## Open Questions

### 1. Calibration / mapping table — resolved (layout)

`NormalizeStick` (`0x4946B0`) indexes two parallel per-device arrays by the axis's device
number (`_joystickXDevice`/`YDevice`/`ThrottleDevice`/`RudderDevice`):

- **`_joystickInfo`** — Win32 `JOYINFO` snapshots, **stride `0x10`** (`&_joystickInfo + dev*0x10`).
- **Calibration table** at base `0x554670` — **stride `0x34` per device**; the raw axis words sit
  at record `+0x00` (X, `0x554670`), `+0x04` (Y, `0x554674`), `+0x0C` (rudder, `0x55467C`). The
  first poll auto-captures the resting value as centre (`_gotCenterX/Y/R` guard →
  `DAT_00554EBC/EC4/EC8`), and `ScaleToRange` (`0x494580`) maps raw→min/centre/max into the
  normalized int the sim consumes.

So each joystick device has a `0x34`-byte calibration record (raw + captured-centre per axis)
plus a `0x10`-byte `JOYINFO` slot; that is the layout the `.CFG` axis bindings persist.

*Status: resolved — re-static (calibration record: 0x34-byte stride/device; JOYINFO 0x10 stride).*

### 2. The Backspace toggle (`gamePrefs` bit `0x4000`)

Backspace flips `gamePrefs` bit `0x4000` in flight, and the in-flight menu exposes the same
bit (item `0x30E`) only below 800-pixel widths — some low-resolution display preference.
The FMENUD menu resource's item label would name it (an asset lookup, not more code
reading).

*Status: open — re-asset (read the FMENUD `.MNU` item labels; see also shell-ui.md § FlightMenu).*

## Related

- [shell-ui.md](shell-ui.md) — the shell screens consuming the same key-code space; the in-flight menu that configures the devices.
- [network.md](network.md) — the serial/modem link transport (SER_/MOD_).
- [physics.md](physics.md) — control inputs feed the flight model.
- [hud.md](hud.md) — slew/padlock controls drive HUD symbology.
- [cockpit-sensors.md](cockpit-sensors.md) — the `CP*` radar/seeker model the sensor keys toggle.
