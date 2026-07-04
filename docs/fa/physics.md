# FA.EXE Flight Model & Stores

The aircraft **flight model** (`FM_*`) and **hardpoint/stores management** (`HARD_*`) —
how the engine turns pilot/AI control inputs and a loadout into per-frame aircraft state:
gear, flaps, wing sweep, thrust vectoring, weapon-bay doors, throttle, fuel burn, weight,
and the rearm/repair bookkeeping behind it. All virtual addresses are from the shipping
binary. Fixed-point values use the engine's standard s8.8 format (divide by 256 for real
units) unless noted.

> **Scope:** this is the [reconstruction-program](reconstruction.md) `flight-model`
> subsystem (#212) — `FM_*` + `HARD_*`, `0x451480–0x454800`. The broader physics story it
> used to bundle lives in adjacent subsystems: **collision** (`_Collision`, `COL_*`) is
> #222, **weapons/projectiles** (`PROJ_*`, the seeker/ballistics model) is #215, and
> **terrain** height/queries (`T_*`) is #221. The sections below that describe those
> (§ Terrain Collision, § PROJ dispatch, § Collision Detection) are provisional and
> migrate to those docs as they land.

> **Provenance:** Ghidra static analysis of FA.EXE with [FA.SMS](formats/SMS.md) symbols
> applied; every flight-model symbol here is recorded in the
> [symbol database](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/flight-model.csv)
> and applied to the Ghidra project. Confidence markers follow
> [spec-authoring.md](../spec-authoring.md): confirmed · inferred · unknown.

---

## Globals — Key Physics State

These globals are polled by nearly every function in the flight model. All are
per-player (the flight model operates on the current object selected by `_curId`).

| Global | Address | Role |
|--------|---------|------|
| `DAT_0050ceb4` | `0x50CEB4` | Current airspeed (s16.8 knots) |
| `DAT_0050ce9f` | `0x50CE9F` | Pitch angle (s16, engine units) |
| `DAT_0050ce95` | `0x50CE95` | Altitude (absolute, engine units — 1 unit = 1 ft) |
| `DAT_0050ceaf` | `0x50CEAF` | Ground elevation below aircraft |
| `DAT_0050ce9d` | `0x50CE9D` | Heading angle |
| `DAT_0050cea1` | `0x50CEA1` | Bank angle |
| `DAT_0050ce91` | `0x50CE91` | X world position |
| `DAT_0050ce99` | `0x50CE99` | Z world position |
| `DAT_0050d06e` | `0x50D06E` | Current throttle position (s16.8, smoothed) |
| `DAT_0050d072` | `0x50D072` | Target throttle % (0–100) |
| `DAT_0050d07c` | `0x50D07C` | Internal fuel quantity (s16.8) |
| `DAT_0050d01b` | `0x50D01B` | G-load (s16.8, 0x100 = 1 G) |
| `DAT_0050d08c` | `0x50D08C` | Stall state byte (0=normal, 1=warning, 2=stall, 3=deep stall, 4=snap) |
| `DAT_0050cfef` | `0x50CFEF` | HUD/state flags word — bit 0x40 = gear down, bit 0x20 = afterburner, bit 0x80 = low fuel, bit 0x200 = bay open |
| `_onTheGround` | (SMS) | Boolean: aircraft on ground |

---

## 1. Flight Model Overview

The player flight model runs inside `_FMFlight@0`. For NPC aircraft the same PT data
is used but fuel burn goes through `@FMBurnNPCFuel@4`.

### `_FMFlight@0` — `0x47B020`

FA.SMS name: `_FMFlight@0`

Main per-frame flight update for the player aircraft. Called from the flying loop.
Entry sequence:

1. `_FMAircraftSetup_0()` — copies PT type fields to the working globals (see §7).
2. `_F24ToPA_4(DAT_0050d013)` → `DAT_0050ce9f` — converts stick pitch to pitch angle.
3. `_SetThrottle_4(_throttle)` — applies throttle input.
4. `_BurnFuel_0()` — deducts fuel for this tick.
5. Gear, gear pitch, weapon bay, thrust vector, and wing sweep update calls.
6. Stall detection: compares `DAT_0050ceb4 >> 8` against `_oneGStallSpeed__3JA`.
   State machine transitions through `DAT_0050d08c` values 0→1→2→4; state 2 applies
   control authority reduction proportional to `(stallSpeed - currentSpeed) / stallSpeed`.
7. Stick inputs fed through `_StickInput_28` for pitch (`DAT_0050d01b`), roll
   (`DAT_0050cfff`), yaw (`DAT_0050d007`), and thrust-vector nozzle
   (`DAT_0050d12f` / `DAT_0050d133`).

### `@FMFuelConsumption@4` — `0x451E50`

FA.SMS name: `@FMFuelConsumption@4`

```c
int _FMFuelConsumption_4(int throttle_pct)
{
    if (100 < throttle_pct)
        return (int)DAT_0050d3cb << 8;      // afterburner fuel flow (PT field)
    return (DAT_0050d3c9 * throttle_pct * 0x100) / 100;  // mil power flow × pct
}
```

Parameters: `throttle_pct` — integer 0–100 (or >100 for afterburner).  
Returns: fuel flow rate in s16.8 units per tick.  
Reads: `DAT_0050d3c9` (mil power fuel flow, PT field), `DAT_0050d3cb` (AB fuel flow, PT field).  
Callers: `_PLANECheckFuel@0` (0x49FB70), `FUN_00451e8b` (0x451E8B — the player fuel tick).

### `_BurnFuel@0` — `0x451E80`

FA.SMS name: `_BurnFuel@0`

Deducts fuel from the internal tank (`DAT_0050d07c`) and from external hardpoint
stores (via `_HARDFindStore_12`). Called directly by `_FMFlight@0` (0x47B020) and
`@FMBurnNPCFuel@4` (0x452050). The player tick wrapper `FUN_00451e8b` calls
`_FMFuelConsumption_4` to get the current flow rate and deducts 5 units per 5-tick
service interval; when an external tank empties it is automatically jettisoned and
the HUD message `"Empty external tanks automatically jettisoned"` is played with the
`_BOMB_11K` sound.

### `@FMBurnNPCFuel@4` — `0x452050`

FA.SMS name: `@FMBurnNPCFuel@4`

NPC fuel burn. `param_1` is the NPC's current fuel quantity. Computes a throttle
percentage from the fuel level relative to PT thresholds (`DAT_0050d0cb` / `DAT_0050d0cf`),
smooths towards the target via `_MatchF24_12`, then calls `_BurnFuel_0()`. Also triggers
the afterburner HUD flag (bit 0x20 of `DAT_0050cfef`) when fuel is critically low and
the PT has an afterburner (`DAT_0050d3bb != 0`).

---

## 2. Aerodynamic Parameters

### Stall

| Symbol | Address | Role |
|--------|---------|------|
| `?oneGStallSpeed@@3JA` | `0x54B72C` | 1-G stall speed (knots, integer) — threshold for stall warning |
| `?stallVol@@3GA` | `0x5718E4` | Stall warning audio volume |
| `?stallString@@3PADA` | `0x4EC9E8` | Stall text string pointer |
| `?stallWarningString@@3PADA` | `0x4EC9F0` | Stall-warning text string pointer |
| `?liftFailString@@3PADA` | `0x4ECA08` | Lift-fail text string pointer |

Stall onset: `_FMFlight@0` checks `(DAT_0050ceb4 >> 8) <= _oneGStallSpeed__3JA + 25`.
The margin of 25 knots above 1-G stall speed is the warning threshold.  
Stall sounds: `_STALL_5K` (looped) at full `_stallVol` during stall state 2;
`_STALLWR_5K` (looped) at 55% volume during state 1 (warning).

Flight envelope check uses `_CheckFlightEnvelope_8` (reads `DAT_0050d32a`–`DAT_0050d32c`
for the altitude-band range), `_GetFlightEnvelope_4`, and `_EnvelopeSpeedLimits_16`.
Return codes from `_CheckFlightEnvelope_8`: 0 = normal, 1 = stall warning,
2 = stall, 3 = deep stall / snap.

### Turn Rate and G

| Function | Address | Role |
|----------|---------|------|
| `_COTurnRate@0` | `0x4780D0` | Returns current sustained turn rate. For aircraft (`_cg == 4`) calls `_GToTurn_8(PT.brv[5], airspeed)`; for ground vehicles returns `DAT_0050d2c5`. |
| `_COBankRate@0` | `0x478090` | Returns current bank rate. For aircraft with `_cg == 4` returns `PT.brv[1] * 0xB6`; caps at 0x7FFF. |
| `_GToTurn_8` | (SMS) | Converts G-load + airspeed to turn-rate. Used in `_FMFlight@0` for `DAT_0050d003` (current turn demand). |
| `_MaxGAtAlt_0` | (SMS) | Returns the flight-envelope index for max-G altitude band; used to look up corner speed via `_GetFlightEnvelope_4`. |
| `_COGPullDrag@0` | `0x4784A0` | Returns G-induced drag increment. Computes `(DAT_0050d3d3 * (gPullLoad * DAT_0050d3e3 / 100 + 100)) / 100`. |

`_COGPullDrag@0` reads two PT drag fields: `DAT_0050d3d3` (base induced drag coefficient)
and `DAT_0050d3e3` (G-drag scaling factor). `DAT_0050d0dd` and `DAT_0050d0df` hold the
accumulated G load across pitch and yaw axes.

### Thrust

`@COThrust@4` (`0x478190`) returns `DAT_0050d3b7` (mil thrust) or `DAT_0050d3bb`
(AB thrust) depending on the `param_1` AB flag. In multiplayer with `_gameMultiPrefs`
bit 0x10 set, thrust is halved for AI opponents.

### Mach

`@SpeedOfSound@4` (`0x412780`) is present in FA.SMS. Mach number is not stored as an
explicit global; the engine scales indicated airspeed from knots using the BRF
fixed-point convention (6,076 units = 1 nm; 1 unit = 1 ft).

### Wing Sweep

`_FMUpdateWingSweep@0` (`0x4515E0`) — for variable-sweep aircraft. Reads
`_COMinSpeed_0()` and `_COMaxSpeed_0()`, linearly interpolates sweep position
`_DAT_0050d021` from 0 to 0x7FFF across the speed range
`[minSpeed, (minSpeed + maxSpeed)/2]`.

---

## 3. `_PROJProc` Virtual Dispatch

### Wrapper — `FUN_004C1F10` (`0x4C1F10`)

No FA.SMS name. Confirmed to read entity field `+0x4` from the PROJ entity. Callers
list in the Ghidra output is empty — the wrapper is invoked indirectly through the
object-update loop's `utilProc` pointer, not via a direct call chain that Ghidra could
trace statically. The architecture.md table maps `_PROJProc` as the `utilProc`
symbol for `.JT` projectile files.

### Dispatch — `0x4C1F50` and `PROJMoveProc` — `0x4C11B0`

Both addresses returned **NOT FOUND** in the Ghidra analysis; neither contains a
decompilable function at those exact VAs. They are likely thunks or mid-function entry
points in the PROJ update path. The closest named functions in the vicinity are:

| VA | Name | Notes |
|----|------|-------|
| `0x4C1120` | `_PROJSpeed@8` | Computes clamped projectile speed |
| `0x4C1170` | `_PROJEngineState@0` | Returns 0/1/2 for motor phase (boost / coast / burnout) |
| `0x4C20C0` | `_PROJHit@8` | Hit detection; reads entity `+0x55` |
| `0x4C2170` | `_PROJFire@16` | Fire initialisation; reads entity `+0x50` |
| `0x4C26F0` | `_PROJFireSound@4` | Fire sound; reads entity `+0x7F` |
| `0x4C2860` | `_PROJInFOV@40` | FOV check; reads entity `+0x50` |

### `_PROJSpeed@8` — `0x4C1120`

```c
int _PROJSpeed_8(int proj_type_ptr, int throttle_pct)
{
    int spd = (*(byte*)(proj_type_ptr + 0x115) * (throttle_pct >> 8)) / 100;
    if (spd < *(short*)(proj_type_ptr + 0xFB)) spd = *(short*)(proj_type_ptr + 0xFB);
    if (spd < *(short*)(proj_type_ptr + 0x67)) spd = *(short*)(proj_type_ptr + 0x67);
    if (*(short*)(proj_type_ptr + 0x6B) < spd)  spd = *(short*)(proj_type_ptr + 0x6B);
    return spd;
}
```

Reads three PROJ_TYPE speed fields at `+0x67` (min speed floor), `+0x6B` (max speed
cap), `+0xFB` (boost minimum), and `+0x115` (throttle scaling byte).

### `_PROJEngineState@0` — `0x4C1170`

Returns the motor phase for the current projectile:
- `0` — boost phase (time since fire < `ram0x0050d367`)
- `1` — sustain phase
- `2` — coast / burnout (time since fire >= `DAT_0050d369`)

Reads `DAT_0050cf68` (fire timestamp) and `ram0x0050d367` / `DAT_0050d369` (phase
durations from the JT type). Turn rates in `_COTurnRate@0` for `_cg == 6` (missile)
dispatch through this state machine: state 0 returns 0, state 1 returns `DAT_0050d36d`,
state 2 returns `_DAT_0050d36f`.

---

## 4. Terrain Collision

### `_GetGround@0` — `0x47AF20`

FA.SMS name: `_GetGround@0`. Reads entity field `+0x1` (confirmed from offset scan).
Returns the ground elevation at the current object's position. Called by terrain
avoidance and landing logic throughout the flight model.

The Ghidra script queried `0x47AF70` under the label `_GetGround@0` but found
`_FMSetTV@8` at that address — the correct VA from the offset scan is `0x47AF20`.

### `_FMSetTV@8` — `0x47AF70`

FA.SMS name: (none found; internal FM helper). Configures thrust-vector nozzle
direction flags `_dirTVY` / `_dirTVZ` and the corresponding `_onTVY` / `_onTVZ`
booleans. Refuses to activate thrust vectoring while `_OnTheGround_0()` is true.
Callers: `?TVKey@@YIGG@Z` (0x413C70), `@FlightKey@4` (0x414690), `_FMFlight@0`
(0x47B020), `_FlightMenu` (0x474800), `_FMResetTV@0` (0x47B000).

### `_T_Info@24` — `0x4ABAB0`

FA.SMS name: `_T_Info@24`. Returns terrain elevation at a world-space point. Called by
`@COLPitchToAvoidTerrain@0` to find the ground level 250 ft ahead of the aircraft:

```c
iVar1 = _T_Info_24(0, NULL, look_ahead_pos, NULL, 0, 0);
clearance = altitude - iVar1 - DAT_0050d2dd;
```

`DAT_0050d2dd` is the terrain avoidance margin.

### `_COLPitchToAvoidTerrain@0` — `0x42DF80`

Computes the pitch command needed to avoid terrain. Projects a point 250 ft ahead
using `_Rotate2_8` and the current heading `DAT_0050ce9d`, then queries `_T_Info_24`.
The required pitch-up is iterated from −90° to +90° (−0x3FFC to +0x3FFC in engine
units) in 0x38E increments until vertical clearance is positive. Returns the cached
pitch value `DAT_0050ceda`; the cache expires based on whether the aircraft is in a
critical regime (`DAT_0050ce9f < -0x71C` or terrain clearance < 0xBB800 = ~3 nm),
refreshed every 1 vs. 4 ticks accordingly.

### `do_use_terrain_detail` — `0x4D2344` and `expandTerrain` — `0x50E145`

Neither address resolved to a function in the Ghidra output. `_expandTerrain` (the
global, not a function) is used as a flag at `0x50E145`-vicinity code: it is set to 1,
`_coarse` is set to 1, a terrain dispatch via `vector_table` fires, then both are
cleared. This confirms `_expandTerrain` is a render-quality hint that forces the
terrain renderer to emit extra-detail geometry for the current tick.

---

## 5. PROJ_TYPE Physics Fields — Offset Scan 0x50–0x7F

Offsets confirmed used by projectile functions. All relative to the PROJ_TYPE base
pointer (the `.JT` BRF data pointer passed as `param_1`).

| Offset | Confirmed by | Likely field |
|--------|-------------|-------------|
| `+0x50` | `_PROJFire@16`, `_PROJInFOV@40`, `FUN_004c2b5a` | Projectile class / type word |
| `+0x55` | `_PROJHit@8` | Hit-detection radius or damage flags |
| `+0x57` | `FUN_004c0a9d` | Proximity fuze or arming distance |
| `+0x60` | `FUN_004c24b0` | Seeker or guidance parameter |
| `+0x64` | `_PROJSpeed@8` | Speed parameter block (also confirmed as `@FMFuelConsumption@4` offset in throttle context) |
| `+0x67` | `_PROJSpeed@8` | Minimum speed (s16) |
| `+0x6B` | `_PROJSpeed@8` | Maximum speed (s16) |
| `+0x7F` | `FUN_004c1c10`, `_PROJFireSound@4` | Fire sound index or audio flags |

The offset-0x64 collision between `@FMFuelConsumption@4` and `_PROJSpeed@8` is not a
conflict: `@FMFuelConsumption@4` uses global `DAT_0050d3c9` (mapped from the PT type
area at `0x50D3xx`) while `_PROJSpeed@8` works on a passed `proj_type_ptr`. They
happen to share the same local offset within their respective type structs.

---

## 6. Collision Detection

### `_Collision@56` — `0x42B800`

FA.SMS name: `_Collision@56`. The main broad-phase + narrow-phase collision query.

Signature:
```c
void _Collision_56(
    ushort   obj_id,         // param_1  — object to test against
    char    *last_hit_cache, // param_2  — optional per-object cache (NULL = no caching)
    byte     flags,          // param_3  — bit 0x80 = use cache; bits 1/4/8/10 = test modes
    int     *pos_a,          // param_4  — start position [x,y,z]
    int     *pos_b,          // param_5  — end position [x,y,z]
    int      radius,         // param_6  — swept-sphere radius
    short    sweep_len,      // param_7  — sweep length hint
    ushort   col_flags,      // param_8  — bit 0x1 = terrain test; bit 0x4 = mesh test; bit 0x200 = no-radius
    ushort  *result_type,    // param_9  — OUT: 0=miss, 0xFFFF=terrain, else mesh-hit index
    int     *hit_pos,        // param_10 — OUT: hit world position [x,y,z]
    ushort  *surface_id,     // param_11 — OUT: mesh surface ID (optional)
    ushort  *poly_id,        // param_12 — OUT: polygon ID (optional)
    short   *hit_normal,     // param_13 — OUT: surface normal (via _COLSetAngle_8)
    int      cache_write     // param_14 — if non-zero, writes back to last_hit_cache
);
```

When `col_flags & 1` is set (terrain test), calls `_COLFlatGround__YIDJPAUF24_POINT3__00_Z`
for the coarse flat-terrain check, then optionally `_T_GetLeaf_12` for the detailed
terrain leaf hit. When `col_flags & 4` is set, calls `FUN_0042c9b0` for the mesh sweep.
When `col_flags & 0xA` is set, calls `FUN_0042c840` for the broad-phase AABB sweep.

The cache (`last_hit_cache`) skips the full geometry query if the object position
matches the previous tick's stored values; cache validity is bounded by
`_currentTicks + 1` or `_currentTicks + 0x100` depending on the hit object's
altitude and armor state.

### `?IntersectT@@YAJPAUF24_POINT@@JJ@Z` — `0x447970`

FA.SMS name (demangled): `IntersectT(F24_POINT*, long, long)`.

Ray-sphere intersection test. Takes a point record, a minimum distance, and a
maximum distance; returns the intersection parameter along the ray or 0 on miss.

```c
int _IntersectT(int *point, int t_min, int t_max)
{
    if (t_min <= point[1])
    {
        int t = _MultDiv32_12(*point >> 8, t_min >> 8, point[1] >> 8);
        if (t < t_max) return t;
    }
    return 0;
}
```

### Terrain flat-ground test — `_COLFlatGround__YIDJPAUF24_POINT3__00_Z`

Called by `_Collision@56` with the object ID, start/end positions, and an output
buffer for the hit point. Returns non-zero on terrain hit; if detailed terrain
(`_th != 0`) is active, calls `_T_GetLeaf_12` to get the surface classification byte
from the T2 tile, which is then passed to `FUN_0042de60` for the landing/damage event.

---

## 7. PT_TYPE (Aircraft Performance Type) — Field Mapping

`_FMAircraftSetup@0` (`0x47A690`) copies PT type data into the working globals at
`0x50D3xx` and `0x50D38x`. The offset scan covers `0x00–0xC0` in the PT type block.
The following offsets are confirmed read by named FM or CO functions:

| Global (after setup) | Source offset in PT | Confirmed by | Role |
|----------------------|---------------------|-------------|------|
| `DAT_0050d3b5` | (PT +?) | `FUN_00451e8b` | Afterburner spool-up time |
| `DAT_0050d3b7` | (PT +?) | `@COThrust@4` | Mil-power thrust (lbs, scaled) |
| `DAT_0050d3bb` | (PT +?) | `@COThrust@4`, `@FMBurnNPCFuel@4` | AB thrust (0 = no AB) |
| `DAT_0050d3bf` | (PT +?) | `FUN_00451e8b` | Throttle ramp-down rate |
| `DAT_0050d3c1` | (PT +?) | `FUN_00451e8b` | Throttle ramp-up rate |
| `DAT_0050d3c3` | (PT +?) | `_FMInitPlane@8` | Min nozzle angle |
| `DAT_0050d3c5` | (PT +?) | `_FMInitPlane@8`, `_ThrustSupport@0` | Max nozzle angle (0 = no TVC) |
| `DAT_0050d3c7` | (PT +?) | `_FMUpdateThrustVector@0` | Nozzle slew rate |
| `DAT_0050d3c9` | (PT +?) | `@FMFuelConsumption@4` | Mil-power fuel flow |
| `DAT_0050d3cb` | (PT +?) | `@FMFuelConsumption@4` | AB fuel flow |
| `DAT_0050d3cd` | (PT +?) | `_FMInitPlane@8` | Initial/full fuel load |
| `DAT_0050d3d3` | (PT +?) | `_COGPullDrag@0` | G-induced drag base coefficient |
| `DAT_0050d3db` | (PT +?) | `_FMInitPlane@8` | Gear-up spawn flag (1 = start airborne gear-up) |
| `DAT_0050d3e3` | (PT +?) | `_COGPullDrag@0` | G-drag scaling factor |
| `DAT_0050d3e5` | (PT +?) | `_FMUpdatePlaneFields@0` | Damage drag penalty scaling |
| `DAT_0050d32a` | (PT +?) | `_FMUpdatePlaneFields@0` | Min flight-envelope altitude band index |
| `DAT_0050d32c` | (PT +?) | `_FMUpdatePlaneFields@0` | Max flight-envelope altitude band index |
| `DAT_0050d322` | (PT +?) | `_FMInitPlane@8`, `_FMUpdatePlaneFields@0` | PT capability flags (bit 3 = carrier, bit 0x1C = TVC mode, bit 0x400 = snap stall) |
| `DAT_0050d3a8` | (PT +?) | `_FMUpdateGearPitch@0` | Gear-pitch authority factor (× 0xB6) |

The flight-envelope data (`_GetFlightEnvelope_4`, `_EnvelopeSpeedLimits_16`,
`_StallSpeed@4`) is indexed by altitude band (integer 0–`DAT_0050d32c`). Each
envelope entry holds a min speed, a max (corner) speed, and a stall speed; the
interpolation in `_FMUpdatePlaneFields@0` produces `DAT_0050d0d7` (min safe speed)
and `DAT_0050d0d9` (best turn speed) every game tick.

`_StallSpeed@4` (`0x49D1D0`) is a thin wrapper:

```c
uint _StallSpeed_4(short *envelope_entry)
{
    uint spd;
    _EnvelopeSpeedLimits_16(envelope_entry, &spd, NULL, NULL);
    return max(spd, 1);
}
```

---

## 8. Dark Zone 0x4D0000–0x4EFFFF

This range is the 3D renderer and rasteriser — not a physics dark zone in the
traditional sense. It has no FA.SMS symbols but contains dense, hand-optimised x86.
Notable functions found:

| VA | Internal name | Role |
|----|--------------|------|
| `0x4D028C` | `FUN_004d028c` | Polygon clip / projection kernel. Takes a packed shift word; applies the 3×3 rotation matrix (`m1`–`m9`, `_scaled_matrix`) to a scaled vertex and tests all six clip planes. Returns a signed distance for the determining clip edge. |
| `0x4D0494` | `get_sort_dist` | Painter's-algorithm sort key. Computes `|xv32| + |yv32| + |zv32|` using abs-and-add approximation plus a per-object size bias from `*(ushort*)(EDI - 0xC) * 0x100`. |
| `0x4D057C` | `_GRAddBrentObj@40` | Adds a BRF object to the render list. Transforms object-relative position to viewer-relative, scales, calls `FUN_004d028c` for the clip test, calls `get_sort_dist`, then writes a 0x30-byte sort entry to `obj_ptr` / `cur_sort_ptr`. |
| `0x4D0798` | `FUN_004d0798` | BRF shape renderer. Saves the rotation matrix and viewer-relative components (`_xv`, `_yv`, `_zv`), calls `_WRSetRemaps_8` for palette remapping, calls `FUN_004ce784` for the perspective divide, dispatches to the shape-type draw routine via `vector_table`, then restores state. |
| `0x4D1694` | `FUN_004d1694` | Terrain object render entry. Computes absolute viewer-relative delta (`*ESI - __viewer_x/y/z`), shift-normalises, applies the rotation matrix, and dispatches to the terrain polygon pipeline. |
| `0x4D715A` | `_DirectDrawCreate@12` | Thin thunk to `DirectDrawCreate` Win32 API — marks the start of the DirectDraw IAT stub block. |

Key renderer globals in this range:

| Global | Role |
|--------|------|
| `_scaled_matrix`, `m2`–`m9` | Current 3×3 rotation matrix (s16 fixed-point) |
| `_xv`, `_yv`, `_zv` | Current vertex in viewer space (s16) |
| `_xv32`, `_yv32`, `_zv32` | Same in s32 for high-precision paths |
| `__viewer_x/y/z` | Camera world position |
| `obj_ptr` / `cur_sort_ptr` | Write cursors into the render / sort list |
| `_expandTerrain` | Set to 1 during the terrain detail render pass |
| `_coarse` | Set to 1 alongside `_expandTerrain` for the detail pass |
| `_overflow` | Set to 0xFFFF when a vertex overflows the clip range |
| `axis_check_type` | Selects the axial clip variant via `PTR_LAB_004d04dc` |

`_expandTerrain` and `_coarse` are not function VAs — they are boolean globals
toggled around the terrain dispatch call. The query for function `expandTerrain @
0x50E145` found them being set at that address as part of a larger terrain-update
function that builds a terrain command buffer before dispatching through `vector_table`.

---

## Summary — Function Quick Reference

| VA | FA.SMS name | Section |
|----|------------|---------|
| `0x42DF80` | `@COLPitchToAvoidTerrain@0` | §4 |
| `0x42B800` | `_Collision@56` | §6 |
| `0x447970` | `?IntersectT@@YAJPAUF24_POINT@@JJ@Z` | §6 |
| `0x451E50` | `@FMFuelConsumption@4` | §1 |
| `0x451E80` | `_BurnFuel@0` | §1 |
| `0x452050` | `@FMBurnNPCFuel@4` | §1 |
| `0x47AF20` | `_GetGround@0` | §4 |
| `0x47AF70` | `_FMSetTV@8` | §4 |
| `0x47B020` | `_FMFlight@0` | §1 |
| `0x478090` | `_COBankRate@0` | §2 |
| `0x4780D0` | `_COTurnRate@0` | §2 |
| `0x478190` | `@COThrust@4` | §2 |
| `0x4784A0` | `_COGPullDrag@0` | §2 |
| `0x4515E0` | `_FMUpdateWingSweep@0` | §2 |
| `0x4ABAB0` | `_T_Info@24` | §4 |
| `0x49D1D0` | `_StallSpeed@4` | §7 |
| `0x49FB70` | `_PLANECheckFuel@0` | §1 |
| `0x4C1120` | `_PROJSpeed@8` | §3 |
| `0x4C1170` | `_PROJEngineState@0` | §3 |
| `0x4C1F10` | `FUN_004c1f10` (PROJ wrapper) | §3 |
| `0x4D028C` | `FUN_004d028c` (clip kernel) | §8 |
| `0x4D0494` | `get_sort_dist` | §8 |
| `0x4D057C` | `_GRAddBrentObj@40` | §8 |
| `0x4D0798` | `FUN_004d0798` (shape renderer) | §8 |

## Update flow

Each frame, the object system services the aircraft entity; for a player/AI plane the
control inputs (throttle, stick, gear/flap/brake/hook/bay commands) are folded into the
mirrored entity state, `FMUpdatePlaneFields` recomputes the derived aircraft state
(weight from the current loadout via `HARDPtrs`, thrust from `FMVector`/throttle, drag
and fuel burn), and the result is written back. Stores management (`HARD_*`) sits beside
it: the loadout on each hardpoint sets the weight `FMGetWeight` sums, and rearm/repair
rebuild the loadout between sorties.

![Flight-model update: control inputs and the current loadout feed FMUpdatePlaneFields, which recomputes weight, thrust, drag and fuel each frame.](diagrams/flight-model.svg)

## Functions

Representative subset; the full record is in
[`db/symbols/flight-model.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/flight-model.csv).

| VA | Symbol | Role |
|----|--------|------|
| `0x4518A0` | `FMInitPlane` | initialise the flight model for a spawned aircraft |
| `0x452140` | `FMUpdatePlaneFields` | per-frame recompute of derived aircraft state |
| `0x4516B0` | `FMGetWeight` | sum airframe + fuel + loadout weight |
| `0x451A60` | `LimitThrottle` | clamp throttle to the flight-envelope / damage limits |
| `0x451B00` | `SetThrottle` | apply a throttle command |
| `0x451B60` | `FMFlaps` | flap deployment state and effect |
| `0x451C90` | `FMGear` | landing-gear deployment |
| `0x451C30` | `FMHook` | arrestor-hook state (carrier ops) |
| `0x451D70` | `FMBrakes` | wheel/speed-brake state |
| `0x451E00` | `FMVector` | thrust-vectoring nozzle angle |
| `0x4515E0` | `FMUpdateWingSweep` | variable-geometry wing sweep update |
| `0x452630` | `FMBay` | weapon-bay door state |
| `0x451E50` | `FMFuelConsumption` | fuel-flow rate from throttle/engine state |
| `0x451E80` | `BurnFuel` | drain fuel from the tanks/stores each tick |
| `0x452770` | `HARDPtrs` | resolve a hardpoint's loaded store record |
| `0x452980` | `HARDCanLoad` | test whether a store may load on a hardpoint |
| `0x452C20` | `HARDLoad` | load a store onto a hardpoint |
| `0x452D90` | `HARDBestSeekers` | rank a target's seeker-capable weapons |
| `0x453220` | `HARDUnrotatedHardPos` | hardpoint position in the airframe frame |
| `0x453890` | `HARDStoreName` | resolve a loaded store's resource-name string |
| `0x453A70` | `HARDTotalFuel` | total fuel including external tanks |
| `0x453B90` | `HARDRearmTest` | can this loadout be rearmed at the current base |
| `0x454140` | `ChangePlaneType` | swap the aircraft to a different type record |
| `0x4543C0` | `SelectRepairPlane` | pick/repair an aircraft in the campaign rearm flow |

## Open Questions

### 1. Fuel-flow scaling constant

`FMFuelConsumption` (`0x451E50`) scales the throttle/engine-state term by a fixed factor
before `BurnFuel` drains it; whether that constant is a global tuning value or derived
from the engine-type record (`_cgt`) is not yet traced. A short look at its one caller
chain will settle it.

*Status: open — re-static.*

## Related

- [reconstruction.md](reconstruction.md) — the program this subsystem belongs to.
- [objects.md](objects.md) — the entity service loop that drives the per-frame FM update
  and owns the `_cg`/`_cgt` mirror the FM state lives in.
- [formats/PT.md](formats/PT.md) — the aircraft performance-type record whose fields feed
  the flight model.
- [shape-selection.md](shape-selection.md) — `PLANEBreakUp` and the destroyed-model swap
  that the damage path triggers.
