# Artificial Intelligence -- Object AI (.AI)

FA_2.LIB contains 9 `.AI` files. Each defines the AI behaviour for one object class using a custom **goto-based scripting language** — plain ASCII text, CRLF line endings. A companion compiled `.BI` file exists for each script.

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

---

## Execution Model

The engine calls each object's AI script once per tick. Execution starts at the top of the file and falls through the opening `if` dispatch chain. The first condition that matches jumps to the corresponding handler label. `exit` returns control to the engine; `restart` re-enters the script from the top on the next tick.

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

---

## Syntax

### Comments
```
; anything after semicolon is a comment
```

### Labels
```
<name>:
```
Any identifier followed by `:`. Labels are local to the file.

### Variables
Four general-purpose integer registers: `%a`, `%b`, `%c`, `%d`.

```
%a = <expr>          ; assign
%a = alt             ; read numeric attribute
%a = random 100      ; random 0–99
%a = h + 45          ; arithmetic on engine values
%a = turnRadius * 3
%a = 50 - random 20
```

### Conditional branches

**Single-line:** `if <condition> goto <label>`

**Block form:**
```
.if <condition>
    <instructions>
.else              ; optional
    <instructions>
.endif
```

Conditions may be combined with `&&`, `||`, and `not`. Trailing `,` is equivalent to `&&` (observed in F.AI).

### Control flow

| Statement | Description |
|-----------|-------------|
| `exit` | Return to engine; script resumes at top next tick |
| `restart` | Re-enter script from top immediately |
| `goto <label>` | Unconditional jump |

---

## Engine State Flags (`do_*`)

Set by the engine before each tick. All 9 files dispatch on at least `do_nothing` through `do_attack`.

| Flag | Meaning |
|------|---------|
| `do_nothing` | No action required |
| `do_ir_launch` | Fire IR (heat-seeking) weapon |
| `do_radar_launch` | Fire radar-guided weapon |
| `do_hit` | Object has been hit |
| `do_evade` | Evade incoming threat |
| `do_attack` | Engage target |

---

## Conditions

### Boolean attributes

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

### Numeric attributes (used in comparisons: `<`, `>`, `<=`, `>=`, `==`)

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

Arithmetic is valid in comparisons: `alt < turnRadius * 3`, `distToTgt < minSpeed + 500`.

### Probability conditions

| Condition | Description |
|-----------|-------------|
| `chance <int>` | True if a global counter equals the value — used as a sparse timer |
| `percent <int>` | True with probability N% (0–100) |
| `random <int>` | Evaluates to a random value 0–(N−1); used in expressions like `random 3 > 1` |

---

## Instructions

### Movement

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `move` | `move <hdg> <alt> <any\|engageP> <corner\|maxSpeed\|cornerSpeed> <value>` | Fly to heading/altitude |
| `moveToAlt` | `moveToAlt <hdg> <alt> maxSpeed <value>` | Climb or descend to altitude |
| `homePos` | `homePos <hdg> <alt> <alt2> corner <value>` | Return to home position |
| `homeAngle` | `homeAngle <hdg> <alt> corner <value>` | Fly to home angle |
| `jink` | `jink <hdg> <alt> <roll> <period> <count> <delay> <speed_mode> <value>` | Jinking evasive maneuver |
| `circle` | `circle <cx> <cy> <cz> <radius> <alt> <speed>` | Orbit a point (AC130 only) |
| `wm_break` | `wm_break <angle> engageP` | Break away from wingman |
| `wm_approach` | `wm_approach <offset> <engageP\|int> corner` | Wingman approach |

### Maneuvers

| Instruction | Description |
|-------------|-------------|
| `maneuver "<name>"` | Execute a named preset maneuver (displayed to player) |
| `immelman corner` | Immelmann turn |
| `invert` | Push-over / invert |
| `yoyo <alt> corner <value>` | Yo-yo maneuver |
| `btoh` | Barrel turn onto heading |

### Control

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `switch` | `switch random <N> <label1> … <labelN>` | Jump to one of N labels chosen uniformly at random |

---

## Named Maneuvers

Maneuver names are trilingual strings: `"<English>;<German>;<French>"`. The UI displays the locale-appropriate segment.

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

Engine-defined `switch` target labels (no `maneuver` string needed): `fastHigh`, `popup`, `offsetPass`, `overheadPass`, `homeOnTgtRear`, `homeAboveBelow`, `straightClimb`, `homeOnTarget`, `straightDive`, `vertScissors`, `split_s`, `immelman`, `breakLeft`, `breakRight`, `h_jink`, `v_jink`, `turnAround`.

---

## Location

| LIB | Count |
|-----|-------|
| FA_2.LIB | 9 |

## TODO — Deep Dive

- Locate the script parser/interpreter in FA.EXE (xref to `.AI` filename loading)
- Confirm full `move` / `jink` argument semantics (heading reference points, speed modes)
- Confirm relationship between `.AI` script and companion `.BI` overlay

## Related

- [BI.md](BI.md) — compiled binary companion, one per `.AI` file
- [BRF.md](BRF.md) — object type records (`.OT`, `.NT`, `.PT`) that reference AI files by name
