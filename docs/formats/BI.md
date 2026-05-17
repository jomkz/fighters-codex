# Compiled AI Runtime (.BI)

FA_2.LIB contains 9 `.BI` files — exactly one per `.AI` script file (e.g. `AC130.BI` paired with `AC130.AI`). Each is a **Win32 PE DLL** that implements the runtime operations referenced by the paired `.AI` script.

## Role in the AI System

The FA engine runs a two-part AI system:

- **`.AI`** — plain-text source script; defines the logic (conditions, branches, actions) in a goto-based language
- **`.BI`** — Win32 PE DLL loaded by `LoadLibrary`; exports the native implementations of every condition and action that the `.AI` interpreter calls

The `.AI` parser reads lines like `if tgtIsPlane goto ...` or `maneuver "CLIMB;..."` and resolves these to exported functions in the paired `.BI`. The `.BI` is therefore the compiled "standard library" for the AI script, not a compiled form of the `.AI` text.

## Exported Functions

All `.BI` files export two families of functions, prefixed `_CT`:

### Action functions (`_CTDo_*`)

Called when the `.AI` script executes an instruction:

| Export | AI instruction |
|--------|---------------|
| `_CTDo_btoh` | `btoh` |
| `_CTDo_circle` | `circle` |
| `_CTDo_exit` | `exit` |
| `_CTDo_homeangle` | `homeAngle` |
| `_CTDo_homepos` | `homePos` |
| `_CTDo_immelman` | `immelman` |
| `_CTDo_invert` | `invert` |
| `_CTDo_jink` | `jink` |
| `_CTDo_maneuver` | `maneuver` |
| `_CTDo_move` | `move` |
| `_CTDo_movetoalt` | `moveToAlt` |
| `_CTDo_restart` | `restart` |
| `_CTDo_wm_approach` | `wm_approach` |
| `_CTDo_wm_break` | `wm_break` |
| `_CTDo_wm_hspacing` | (internal wingman spacing) |
| `_CTDo_yoyo` | `yoyo` |

Simpler `.BI` files (AC130.BI) export only the subset of actions used by that aircraft class.

### Condition functions (`_CTEval_*`)

Called when the `.AI` script evaluates a condition:

| Export | AI attribute / condition |
|--------|--------------------------|
| `_CTEval_alt` | `alt` |
| `_CTEval_altdiff` | `altDiff` |
| `_CTEval_b` | `b` (internal counter) |
| `_CTEval_betterspeed` | `betterSpeed` |
| `_CTEval_bettertwr` | `betterTwr` |
| `_CTEval_canclimb` | `canClimb` |
| `_CTEval_corner` | `corner` / `cornerSpeed` |
| `_CTEval_disttotgt` | `distToTgt` |
| `_CTEval_do_attack` | `do_attack` |
| `_CTEval_do_evade` | `do_evade` |
| `_CTEval_do_hit` | `do_hit` |
| `_CTEval_do_ir_launch` | `do_ir_launch` |
| `_CTEval_do_nothing` | `do_nothing` |
| `_CTEval_do_radar_launch` | `do_radar_launch` |
| `_CTEval_engagep` | `engageP` |
| `_CTEval_h` | heading |
| `_CTEval_hdiff` | `hdiff` |
| `_CTEval_hrzdisttotgt` | `hrzDistToTgt` |
| `_CTEval_htotgt` | heading-to-target |
| `_CTEval_maxspeed` | (max speed attribute) |
| `_CTEval_minspeed` | `minSpeed` |
| `_CTEval_p` | `p` |
| `_CTEval_pdiff` | `pdiff` |
| `_CTEval_skill` | `skill` |
| `_CTEval_speed` | `speed` |
| `_CTEval_speeddiff` | `speedDiff` |
| `_CTEval_tgt` | `tgt` |
| `_CTEval_tgtahead` | `tgtAhead` |
| `_CTEval_tgtfacing` | `tgtFacing` |
| `_CTEval_tgthumancontrol` | `tgtHumanControl` |
| `_CTEval_tgtisfighter` | `tgtIsFighter` |
| `_CTEval_tgtisplane` | `tgtIsPlane` |
| `_CTEval_tgtoffbeam` | `tgtOffBeam` |
| `_CTEval_turnradius` | `turnRadius` |
| `_CTEval_wingapproach` | `wingApproach` |
| `_CTEval_wingcombat` | `wingCombat` |
| `_CTEval_wm_hspacing_is` | (wingman horizontal spacing) |

### Maneuver name strings

The trilingual maneuver name strings (e.g. `"GND ATTACK;BODENANGRIFF;ATTAQUE AU SOL"`) documented in [AI.md](AI.md) are embedded as data inside `F.BI` (the primary fighter AI runtime). `_CTDo_maneuver` reads these strings and passes the locale-appropriate segment to the UI.

## Size

All 9 `.BI` files are **4608 bytes** decompressed (DCL-compressed in the LIB).

## Location

| LIB | Count |
|-----|-------|
| FA_2.LIB | 9 |

## Related

- [AI.md](AI.md) — plain-text AI script; uses the exports of the paired `.BI` as its instruction set
