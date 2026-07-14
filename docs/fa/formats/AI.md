---
format: AI
name: Object AI Script
extensions: [".AI"]
category: mission
endianness: none
spec:
  status: complete
codec:
  direction: round-trip
  byte_identical: false
  rationale: "AI↔BI round-trips semantically: `fx ai compile` parses the script and `fx bi decompile` regenerates equivalent source that recompiles byte-identically, but comments and the original identifier/label spelling are discarded, so the recovered text is not byte-identical to the authored source"
  lib: [lib/src/ai.cpp]
  commands: [ai]
  tests: [tests/test_ai.cpp]
  fuzz: [fuzz/fuzz_ai.cpp]
  gui: [gui/src/editors/ai_editor.cpp]
  fixtures:
    synthetic: true
    real_manifest: true
    real_install: true
related: [BI, BRF]
---

# AI — Object AI Script (.AI)

Each `.AI` file defines the AI behaviour for one object class using a custom
**goto-based scripting language** — plain ASCII text, CRLF line endings, 9
files in FA_2.LIB. A companion compiled `.BI` file exists for each script of
the same base name — see [BI.md](BI.md).

## Tools

### fx

```
fx ai compile   <file.AI> -o <file.BI>  # full parse + compile to BI bytecode DLL
fx bi decompile <file.BI>               # recover AI source from fx-compiled bytecode
```

`fx ai compile` is a complete parser/validator for the language documented
below. `fx bi decompile` ([BI.md](BI.md)) is its inverse: it reconstructs AI
source whose recompilation is byte-identical to the input, closing the
compile→decompile→recompile loop for every BI `fx` produces. Synthesized labels
(`L####`) replace the original label names, and comments are not recovered.

## File Layout

Plain text; no binary fields.

### Execution Model

The engine calls each object's AI script once per tick. Execution starts at
the top of the file and falls through the opening `if` dispatch chain. The
first condition that matches jumps to the corresponding handler label. `exit`
returns control to the engine; `restart` re-enters the script from the top on
the next tick.

```
; top-level dispatch (evaluated every tick)
if do_nothing goto nothing
if do_ir_launch goto ir_launch
...
exit

nothing:
  exit

ir_launch:
  <instructions>
  restart
```

### Syntax

**Comments:**
```
; anything after semicolon is a comment
```

**Labels:** any identifier followed by `:`. Labels are local to the file.

**Variables:** four general-purpose integer registers: `%a`, `%b`, `%c`, `%d`.

```
%a = <expr>          ; assign
%a = alt             ; read numeric attribute
%a = random 100      ; random 0–99
%a = h + 45          ; arithmetic on engine values
%a = turnRadius * 3
%a = 50 - random 20
```

**Conditional branches** — single-line: `if <condition> goto <label>`;
block form:
```
.if <condition>
    <instructions>
.else              ; optional
    <instructions>
.endif
```

Conditions may be combined with `&&`, `||`, and `not`. Trailing `,` is
equivalent to `&&` (observed in F.AI).

**Control flow:**

| Statement | Description |
|-----------|-------------|
| `exit` | Return to engine; script resumes at top next tick |
| `restart` | Re-enter script from top immediately |
| `goto <label>` | Unconditional jump |

### Engine State Flags (`do_*`)

Set by the engine before each tick. All 9 files dispatch on at least
`do_nothing` through `do_attack`.

| Flag | Meaning |
|------|---------|
| `do_nothing` | No action required |
| `do_ir_launch` | Fire IR (heat-seeking) weapon |
| `do_radar_launch` | Fire radar-guided weapon |
| `do_hit` | Object has been hit |
| `do_evade` | Evade incoming threat |
| `do_attack` | Engage target |

### The complete vocabulary — from the symbol table

An AI program is a tree of **conditions** (`_CTEval_<name>`) and **actions** (`_CTDo_<name>`),
and both compile to calls the `.BI` overlay imports from the game executable. So the language
is not a matter of opinion: it is exactly the `_CT*` symbols the engine exports, all
103 of them, every one claimed in [`db/symbols/`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/ai.csv).

**Bold** = imported by at least one of the nine stock scripts (55 of 103).
The rest are exported by the engine and used by nothing shipped — they are no less real, and
`fx ai compile` accepts them all.

**Actions (26)** — `_CTDo_*`:

**`btoh`** · **`circle`** · **`exit`** · **`homeangle`** · **`homepos`** · **`immelman`** · **`invert`** · **`jink`** · **`maneuver`** · **`move`** · **`movetoalt`** · `play` · `print` · `printnum` · **`restart`** · `rudder` · `splits` · `turn` · `uhomepos` · **`wm_approach`** · **`wm_break`** · `wm_control` · `wm_formation` · **`wm_hspacing`** · `wm_vspacing` · **`yoyo`**

**Conditions (77)** — `_CTEval_*`:

**`alt`** · **`altdiff`** · `any` · **`b`** · `bestrange` · `bestrangediff` · **`betterspeed`** · **`bettertwr`** · **`canclimb`** · `cloudalt` · **`corner`** · **`cornerspeed`** · `cornerspeeddiff` · **`disttotgt`** · `disttowaypoint` · **`do_attack`** · **`do_evade`** · **`do_hit`** · **`do_ir_launch`** · **`do_nothing`** · **`do_radar_launch`** · **`engagep`** · **`h`** · **`hdiff`** · **`hrzdisttotgt`** · **`htotgt`** · `ir` · `maxalt` · `maxaltdiff` · `maxrange` · `maxrangediff` · `maxrudderh` · `maxrudderp` · **`maxspeed`** · `maxspeeddiff` · `minalt` · `minaltdiff` · **`minspeed`** · `minspeeddiff` · **`p`** · **`pdiff`** · `ptotgt` · `radar` · **`skill`** · **`speed`** · **`speeddiff`** · **`tgt`** · **`tgtahead`** · `tgtaspectangle` · `tgtattackinganyone` · `tgtattackingme` · `tgtclass` · **`tgtfacing`** · **`tgthumancontrol`** · `tgtir` · `tgtisaaa` · `tgtisbomber` · **`tgtisfighter`** · **`tgtisplane`** · `tgtissam` · **`tgtisship`** · **`tgtoffbeam`** · `tgtradar` · `time` · **`turnradius`** · `turnradiusdiff` · `turnrate` · `turnratediff` · `twr` · `twrdiff` · `waypointalt` · **`wingapproach`** · **`wingcombat`** · `wm_control_is` · `wm_formation_is` · **`wm_hspacing_is`** · `wm_vspacing_is`

> The compiler's action table used to hold **15** — the ones the stock scripts use, plus
> `turn`. So nine real actions (`play`, `print`, `printnum`, `rudder`, `splits`, `uhomepos`,
> `wm_control`, `wm_formation`, `wm_vspacing`) **could not be compiled at all**: `fx ai compile`
> answered *"unknown statement"* to a word the engine implements. The table was a record of the
> corpus, not of the language. Arity is now read from the source line — the decompiler always
> read it from the bytecode — so any action the engine exports compiles, and
> `tests/test_ai.cpp` pins the accepted set to `db/` so it cannot drift again
> ([#491](https://github.com/jomkz/fighters-codex/issues/491)).
>
> Conditions were always generic (any identifier becomes `_CTEval_<name>`), so all
> 77 already worked.


### Conditions

**Boolean attributes:**

| Attribute | Description |
|-----------|-------------|
| `tgt` | Has an active target |
| `tgtAhead` | Target is within forward arc |
| `tgtFacing` | Target is facing this object |
| `tgtIsPlane` | Target is an aircraft (not ground unit) |
| `tgtIsFighter` | Target is fighter class |
| `tgtHumanControl` | Target is human-controlled |
| `canClimb` / `canclimb` | Aircraft has energy to climb |
| `betterSpeed` | Speed advantage over target |
| `betterTwr` | Thrust-to-weight advantage over target |
| `wingCombat` | Wingman is currently engaged |
| `wingApproach` | Wingman is on approach |

**Numeric attributes** (used in comparisons: `<`, `>`, `<=`, `>=`, `==`):

| Attribute | Description |
|-----------|-------------|
| `alt` | Current altitude (feet) |
| `speed` | Current airspeed |
| `distToTgt` | 3D distance to target |
| `hrzDistToTgt` | Horizontal distance to target |
| `altDiff` / `altdiff` | Altitude difference vs target |
| `hdiff` | Heading difference vs target |
| `pdiff` | Pitch difference vs target |
| `tgtOffBeam` | Target off-beam angle (degrees) |
| `speedDiff` | Speed difference vs target |
| `turnRadius` | Current turn radius |
| `minSpeed` | Minimum flyable speed |
| `skill` | AI skill level (integer, 0 = lowest) |
| `b` | Internal counter (used in loop constructs) |
| `p` | Internal parameter |

Arithmetic is valid in comparisons: `alt < turnRadius * 3`,
`distToTgt < minSpeed + 500`.

**Probability conditions:**

| Condition | Description |
|-----------|-------------|
| `chance <int>` | True if a global counter equals the value — used as a sparse timer |
| `percent <int>` | True with probability N% (0–100) |
| `random <int>` | Evaluates to a random value 0–(N−1); used in expressions like `random 3 > 1` |

### Instructions

**Movement:**

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `move` | `move <hdg> <angle> <alt> <speed_mode> <duration>` | Fly to heading/altitude. `<angle>` = bank angle (0 = wings level, 180 = inverted); `<alt>` scaled ×0xb6 internally (0x7fffffff = altitude-unlimited); `<speed_mode>` = one of `corner`/`max`/etc.; `<duration>` = integer 0–15. Confirmed from `_CTDo_move` (0x465cc0) bytecode pop order: heading, angle, altitude, speed, duration. |
| `moveToAlt` | `moveToAlt <hdg> <alt> maxSpeed <value>` | Climb or descend to altitude |
| `homePos` | `homePos <hdg> <alt> <alt2> corner <value>` | Return to home position |
| `homeAngle` | `homeAngle <hdg> <alt> <speed_mode> <roll> <value>` | Fly to home angle |
| `jink` | `jink <hdg> <defl_angle> <defl1> <defl2> … <count> <speed_mode> <duration>` | Jinking evasive maneuver. `<defl_angle>` = base deflection; `<defl1>`/`<defl2>` = alternating left/right deflection magnitudes; `<count>` = jink repetitions 0–4; `<speed_mode>` = speed control (same domain as `move` speed — clamped to COMinSpeed..COMaxSpeed by `FUN_00465e00`); `<duration>` = integer 0–15. Confirmed from `_CTDo_jink` (0x4663f0) → `_MVRJink@40` (0x4ac9e0): param_8=count, param_9=speed, param_10=duration, param_3/param_4=deflection angles. |
| `circle` | `circle <cx> <cy> <cz> <radius> <alt> <speed>` | Orbit a point (AC130 only) |
| `wm_break` | `wm_break <angle> engageP` | Break away from wingman |
| `wm_approach` | `wm_approach <offset> <engageP\|int> corner` | Wingman approach |

**`move` roll argument**: `any` = no bank constraint (engine picks optimal);
`0` = wings level (upright); `180` = inverted. Comments in `F.AI` confirm:
`move %a 0 180 corner 1` = "roll over on my back, staying horizontal";
`move %a + 180 0 0 corner 0` = "roll out to level". The `engageP` keyword is a
valid value for the `<alt>` argument (altitude of the engage/attack waypoint),
not the roll argument.

**Maneuvers:**

| Instruction | Description |
|-------------|-------------|
| `maneuver "<name>"` | Execute a named preset maneuver (displayed to player) |
| `immelman corner` | Immelmann turn |
| `invert` | Push-over / invert |
| `yoyo <alt> corner <value>` | Yo-yo maneuver |
| `btoh` | Barrel turn onto heading |

**Control:**

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `switch` | `switch random <N> <label1> … <labelN>` | Jump to one of N labels chosen uniformly at random |

### Named Maneuvers

Maneuver names are trilingual strings: `"<English>;<German>;<French>"`. The UI
displays the locale-appropriate segment.

| Name |
|------|
| `"BREAK LEFT;LINKS ABDREHEN;APPROCHE GAUCHE"` |
| `"BREAK RIGHT;RECHTS ABDREHEN;APPROCHE DROITE"` |
| `"CLIMB;STEIGEN;MONTEE"` |
| `"DIVE BOMB;STURZANGRIFF;BOMBARDER"` |
| `"DIVE;STURZFLUG;PIQUE"` |
| `"FAST-HIGH;SCHNELL-HOCH;APPROCHE DU HAUT"` |
| `"GND ATTACK;BODENANGRIFF;ATTAQUE AU SOL"` |
| `"LOOP;LOOPING;BOUCLE"` |
| `"OFFSET PASS;VORBEIFLUG SEITE;PASSE LATERALE"` |
| `"OVERHEAD PASS;VORBEIFLUG OBEN;PASSE HAUTE"` |
| `"OVERSHOOT;ÜBERSCHUSS;OVERSHOOT"` |
| `"POP-UP;POP-UP;ATTAQUE SURPRISE"` |
| `"PURSUIT;VERFOLGUNG;POURSUITE"` |
| `"REVERSE;ABSCHWUNG;180"` |
| `"SCISSORS;SCISSORS;CISEAUX"` |
| `"SEPARATE;LÖSEN;SEPARATION"` |
| `"SPLIT-S;SPLIT-S;IMMELMANN"` |
| `"STRAIGHT;GERADEAUS;TOUT DROIT"` |
| `"UNDERNEATH PASS;VORBEIFLUG UNTEN;AU-DESSOUS"` |
| `"VERT SCISSORS;VERTIKALSCHERE;CISEAUX VERTIC."` |

Engine-defined `switch` target labels (no `maneuver` string needed):
`fastHigh`, `popup`, `offsetPass`, `overheadPass`, `homeOnTgtRear`,
`homeAboveBelow`, `straightClimb`, `homeOnTarget`, `straightDive`,
`vertScissors`, `split_s`, `immelman`, `breakLeft`, `breakRight`, `h_jink`,
`v_jink`, `turnAround`.

## File Inventory

| File | Size | Object class |
|------|------|--------------|
| AC130.AI | 3,728 B | AC-130 Spectre gunship |
| B.AI | 3,970 B | Bomber |
| F.AI | 20,616 B | Fighter (primary; shared by most aircraft) |
| F117.AI | 18,823 B | F-117 stealth (based on F.AI) |
| H.AI | 12,412 B | Helicopter |
| HYDRO.AI | 1,816 B | Hydrofoil / fast patrol boat |
| LARGE.AI | 960 B | Large ship |
| LINER.AI | 917 B | Ocean liner |
| MOTH.AI | 18,422 B | Moth (variant of F.AI) |

All 9 live in FA_2.LIB.

## Engine Notes

The `_CTDo_*` and `_CTEval_*` condition/action dispatcher functions exist in
**the game executable itself** at VA range **0x464C80–0x467110** — not only in the
companion `.BI` DLL files. This means the interpreter core is compiled into
the main executable; the `.BI` DLLs supply per-object script data but delegate
dispatch back to the game executable's built-in handlers.

## Related

**Formats:** [BI](BI.md) — compiled binary companion, one per `.AI` file;
[BRF](BRF.md) — object type records (`.OT`, `.NT`, `.PT`) that reference AI
files by name.
