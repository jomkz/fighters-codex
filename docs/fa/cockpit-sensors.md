# Cockpit sensors — radar / IR / RWR

The **sensor simulation**: what the player is allowed to see. Every frame the engine scores
every object into a radar and an infra-red signature, decides which sensors can detect it, and
files the survivors into the scope buffers the cockpit MFDs draw from. This is the *producer*
of the [HUD](hud.md) symbology and the *input* to the [weapons](weapons.md) seeker/lock model —
both of which were documented while the model that feeds them was not (#486).

> **Provenance:** Ghidra static analysis of the game executable with [FA.SMS](formats/SMS.md)
> symbols applied; the `CP*` functions are recorded in the
> [symbol database](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/cockpit-sensors.csv)
> and applied to the Ghidra project. This subsystem is **complete** — the RCS model, the three
> detection predicates, the RWR timing, and the scope renderers are traced (#486) and every
> function in its range is claimed; a few per-MFD-type layout attributions of the
> `CPScopeHelper*` primitives remain as a follow-up. Confidence markers follow
> [spec-authoring.md](../spec-authoring.md): confirmed · inferred · unknown.

![Cockpit sensor model: every object is scored by CPComputeRCS into a radar and IR signature, gated by CPAddItemToScopes into the contact buffers, which CPDraw renders to the scopes and the weapons seeker consumes.](diagrams/cockpit-sensors.svg)

## The model

**1. Signature — `_CPComputeRCS@8` (`0x43E8C0`).** `CPComputeRCS(entity *target, int mode)`
returns the target's radar signature and computes its IR signature alongside, summing
contributions from:

- **base size** — read from the target's type record (`entity+5` → type field `+0x45`), bucketed
  into small / medium / large;
- **configuration** — the `+0x16F` HUD/state flags: gear down (`0x40`), weapon-bay open (`0x200`),
  and the afterburner/heat bit (`0x80`) each add to both signatures;
- **aspect** — pitch (`entity+0x1F`) and bank (`entity+0x21`) shift the signature with viewing angle;
- **damage** — a damaged airframe (`entity+0x10 & 0x80`, past half damage) is louder on both bands;
- **class extension** — `entity+0xDE & 0x10` (the per-class extension, #476) adds a fixed increment.

**2. Detection gate — `@CPAddItemToScopes@4` (`0x43DEE0`).** Per candidate object it runs three
independent detection tests (radar, IR, RWR) and inserts each pass into the matching contact buffer
via a shared insert routine. Two buffers hold the results: the radar+IR scope list at `0x53BEA8`
and the RWR list at `0x539E58` (each `0x6D6` = 1750 dwords).

**3. Per-frame drivers.** `_CPUpdateRadar@0` (`0x43E810`) timestamps the sweep and clears the
accumulator; `?CPUpdateIRItems` (`0x440FE0`) refreshes the IR-seeker contact list;
`_CPResetRWR@0` (`0x43E830`) zeroes both buffers and re-arms the scan timers.

**4. Presentation — `_CPDraw@8` (`0x439220`).** Renders the radar/IR/RWR scopes and the cockpit
MFDs from the contact buffers, through the window dispatcher below.

## Detection predicates (#486)

`CPAddItemToScopes` runs exactly three independent predicates per candidate object, and files
each pass through the shared `CPScopeInsert` (dedup-and-update by object id): confirmed

- **`CPRadarSees`** (`0x43DF70`) — the player's own radar. The target must be alive and
  radar-detectable (type field `+9` bit `0x20`), pass the **mode-specific look-down filter**
  (`_radarMode` at `0x5387D8` — its value selects ground-clutter handling and the scan-bounds
  box, `0x5387E0` look-up vs `0x539D78` look-down), lie inside the **radar-beam FOV**
  (`PROJInFOV`), and not be **terrain-blocked** (`COLTerrainBlocking`). Detection range is set
  by the target's **RCS** at type `+0x3B` (`RCS × 0x30000` vs range for the wide mode), so the
  same `CPComputeRCS` signature that draws the return also gates whether it is seen. Chaff
  (a `GRAPHIC` of type `0xC`/`0xD`) always paints.
- **`CPSuppRadarSees`** (`0x43E220`) — the AWACS/GCI **datalink**, active only when
  `UsingSuppRadar`. A wider bounds box, filed into the same radar scope as a datalink contact.
- **`CPRwrSees`** (`0x43E330`) — the RWR, i.e. *what is illuminating you*. A SAM/AAA site
  (class 6) registers when its emitter is on (type `+0xA6` bit 1), it is not in a non-emitting
  state, its emitter name is not excluded, and it is within `0x76AC00`; a hostile plane
  (class 2/4) registers when it is radar-capable and carries the **radar-locking-me** flag
  (`entity+0xDE` bit `0x400`). This is why the RWR shows threats the plain radar scope does not.

## Scopes & the RWR display (#486)

`CPDraw` renders each cockpit MFD through **`CPDrawWindow`** (`0x43A190`), a dispatcher that
switches on `_windowTypes[i]`: the radar B-scope / target-designation views route to
**`CPDrawRadarScope`** (`0x43A5C0`, the largest renderer — it walks the radar buffer, resolves
the designated target and the selected-weapon lock, and is itself **weather-gated** through
`WRCanSee`, see [renderer.md](renderer.md#weather-atmosphere-and-visibility-493)), and window
type 10 routes to **`CPDrawRWR`** (`0x43EA40`). The RWR display refreshes every `0x40` ticks and
draws the threat ring plus the aircraft's own **RCS diamond** from `_frontRCS`/`_sideRCS`; each
contact is coloured by lock state — a **track lock** is red (`0x2B`) and a **search lock**
yellow (`0x2C`) until `_currentT` passes `_trackLockEndT` / `_searchLockEndT`. That pair of
timers is the **RWR spike timing**: the launch/lock warning holds for the interval the emitter
keeps painting you. `WPCInit` (`0x438870`) lays out the MFD window rectangles from the screen
resolution, and `CPNextTarget` (`0x440E10`) cycles the radar buffer for the `t`/`T` keys. confirmed

## Functions

Full record: [`db/symbols/cockpit-sensors.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/cockpit-sensors.csv).

| VA | Symbol | Role |
|----|--------|------|
| `0x43E8C0` | `CPComputeRCS` | radar-cross-section + IR-signature model |
| `0x43DEE0` | `CPAddItemToScopes` | radar/IR/RWR detection gate → contact buffers |
| `0x43E780` | `CPRemoveItemFromScopes` | drop a contact from the scopes |
| `0x43E810` | `CPUpdateRadar` | per-frame radar sweep state |
| `0x440FE0` | `CPUpdateIRItems` | per-frame IR-contact refresh |
| `0x43E830` | `CPResetRWR` | clear the scope + RWR buffers, re-arm timers |
| `0x439220` | `CPDraw` | render the scopes / cockpit MFDs |
| `0x438B70` | `CPInit` | allocate buffers, reset the scopes |
| `0x43DDD0` | `CPRadarRange` | current radar range setting |
| `0x43E7E0` | `CPBombRange` | current bomb-range / CCIP setting |
| `0x43DE10` | `UsingSuppRadar` | is the supplementary radar in use |
| `0x43DE90` | `CPSetSkill` | radar/AI skill level |
| `0x438520` | `CPSetMissile` | set the selected missile on the scope |
| `0x43DF70` | `CPRadarSees` | radar detection predicate (look-down / FOV / terrain / RCS range) |
| `0x43E330` | `CPRwrSees` | RWR detection predicate (is this emitter illuminating me) |
| `0x43A190` | `CPDrawWindow` | cockpit MFD window dispatcher (per `_windowTypes[i]`) |
| `0x43A5C0` | `CPDrawRadarScope` | radar B-scope / target-designation MFD (weather-gated) |
| `0x43EA40` | `CPDrawRWR` | RWR threat display + track/search lock spike timing |
| `0x440E10` | `CPNextTarget` | cycle the radar contact buffer (`t`/`T` keys) |

## Open Questions

### 1. `CPDraw` home — sensor or HUD? (resolved: stays)

`_CPDraw@8` (`0x439220`) and the `CPDrawWindow`/`CPDraw*Scope` family render the MFDs, which is
symbology work like [hud.md](hud.md). But the read (#486) shows the renderers are inseparable
from the contact buffers and the detection state they walk — `CPDrawRadarScope` resolves the
designated target and the selected-weapon lock, `CPDrawRWR` reads the lock timers — so the draw
side belongs with the sensor model that produces it, not with the flat HUD symbology. It stays
in this subsystem.

*Status: resolved — re-static (#486; the scope renderers are sensor-coupled, kept here).*

### 2. Detection thresholds & RWR spike timing — resolved

The three predicates and the RWR timing are now read (see § Detection predicates and
§ Scopes): radar detection is a look-down-mode + beam-FOV + terrain-blocking + RCS-range test,
the RWR shows emitters carrying the radar-locking-me flag, and the spike timing is the
`_trackLockEndT` / `_searchLockEndT` pair (red track lock `0x2B`, yellow search lock `0x2C`).
The remaining `CPScopeHelper*` primitives are named but their individual MFD layouts (which
window type is the HSI, the TID, etc.) are only partially attributed.

*Status: resolved — re-static (#486; per-MFD-type layout attribution is a follow-up).*

## Related

- [weapons.md](weapons.md) — the seeker/lock model that consumes this state (`PROJTargetSignal`, `PROJIRSensorOn`, `PROJInNotch`).
- [hud.md](hud.md) — the cockpit symbology that draws the sensor output.
- [structs.md](structs.md) — the entity fields `CPComputeRCS` reads (`+0x16F` flags, `+0x1F`/`+0x21` aspect, `+0xDE` extension).
