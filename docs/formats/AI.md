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
| `move` | `move <hdg> <alt> <roll> <speed_mode> <value>` | Fly to heading/altitude at given bank angle |
| `moveToAlt` | `moveToAlt <hdg> <alt> maxSpeed <value>` | Climb or descend to altitude |
| `homePos` | `homePos <hdg> <alt> <alt2> corner <value>` | Return to home position |
| `homeAngle` | `homeAngle <hdg> <alt> <speed_mode> <roll> <value>` | Fly to home angle |
| `jink` | `jink <hdg> <alt> <roll> <period> <count> <delay> <speed_mode> <value>` | Jinking evasive maneuver |
| `circle` | `circle <cx> <cy> <cz> <radius> <alt> <speed>` | Orbit a point (AC130 only) |
| `wm_break` | `wm_break <angle> engageP` | Break away from wingman |
| `wm_approach` | `wm_approach <offset> <engageP\|int> corner` | Wingman approach |

**`move` roll argument**: `any` = no bank constraint (engine picks optimal); `0` = wings level (upright); `180` = inverted. Comments in `F.AI` confirm: `move %a 0 180 corner 1` = "roll over on my back, staying horizontal"; `move %a + 180 0 0 corner 0` = "roll out to level". The `engageP` keyword is a valid value for the `<alt>` argument (altitude of the engage/attack waypoint), not the roll argument.

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

## .BI Companion Files

Each `.AI` script has a companion `.BI` file of the same base name. All `.BI` files are **Win32 PE DLLs** (`MZ` header) — compiled bytecode loaded by the engine at runtime via `LoadLibrary`. The `.AI` text is the source; `.BI` is the compiled form. File sizes:

### BI Bytecode Interpreter (Confirmed)

The interpreter is `_CTExecProgram@4` (FUN_00466970). It executes at most 5000 opcodes per call, then forcibly invokes `CTDo_exit` to prevent infinite loops.

**Runtime state globals:**

| Global | Role |
|--------|------|
| `DAT_00546bea` | Instruction pointer — `char*` into the loaded BI CODE section |
| `DAT_00546bf0` | Current script priority level (compared against `param_1` passed by caller) |
| `DAT_00546c94` | Pointer to the current actor's live object record |
| `DAT_00546c88` | Actor type flag: 1 if actor type is 2 or 4 (fighter/bomber) |
| `DAT_00546c90` | Execution result returned to caller (non-zero = script performed an action) |
| `DAT_00546c98` | Halt flag — set non-zero to stop execution early |
| `DAT_0050cf6e` | Current actor slot index (0 = player) |
| `DAT_0050d312` | CT system enable flag — interpreter is a no-op when this is zero |

**End-of-program marker:** `'%'` (0x25) — the main loop checks `*ip != '%'` as its loop condition.

**State save/restore:** `FUN_004668f0` restores a 32-dword CT state block from `DAT_0050cf90` (or zeroes it if no saved state). `FUN_00466920` saves the current state back. This enables preemptible script execution with re-entry.

**Opcode dispatch:** `FUN_00466a80` (0x466a80) reads one opcode byte from `*DAT_00546bea` and dispatches. Full opcode table decoded below.

**Evaluation stack:** `FUN_00466290` = push; `FUN_00465ad0` = pop. Stack base = `DAT_00546bf2`; depth = `DAT_00546c42`. Max depth = 32 dwords. `FUN_00466820` reports error codes (1=syntax error, 4=stack underflow, 5=stack overflow, 0xa=unknown opcode, 0xb=call by name to unknown proc, 0xc=stack imbalance).

**Base address:** `DAT_00546be6` is the base pointer for the loaded BI CODE section; all jump offsets are relative to this base.

### BI Bytecode Opcode Table (Confirmed)

| Opcode | IP advance | Name | Description |
|--------|-----------|------|-------------|
| `0x00` | 1 | NOP | No operation |
| `0x25` ('%') | — | END | End of program (also the main-loop terminator) |
| `0x01` | 5 | PUSH_DWORD | Push *(int32*)(IP+1) |
| `0x02` | 3 | PUSH_WORD | Push *(int16*)(IP+1) sign-extended |
| `0x03` | 2 | PUSH_BYTE | Push *(int8*)(IP+1) sign-extended |
| `0x04` | 1 | EVAL | Call FUN_00465ad0 (pop eval-stack top) |
| `0x05` | 2 | STORE_VAR | Pop → var[*(byte*)(IP+1)] via FUN_004670e0 |
| `0x06` | 2 | LOAD_VAR | Push var[*(byte*)(IP+1)] via FUN_004670e0 |
| `0x07` | varies | PUSH_ADDR | Push (IP+1 − base); advance IP past null-terminated string name |
| `0x08` | 1 | MUL | Pop a, b; push b×a |
| `0x09` | 1 | DIV | Pop a, b; push b/a (returns 0 if a=0) |
| `0x0A` | 1 | MOD | Pop a, b; push b%a (returns 0 if a=0) |
| `0x0B` | 1 | ADD | Pop a, b; push b+a |
| `0x0C` | 1 | SUB | Pop a, b; push b−a |
| `0x0D` | 1 | AND | Pop a, b; push b&a (bitwise) |
| `0x0E` | 1 | OR | Pop a, b; push b|a (bitwise) |
| `0x0F` | 1 | XOR | Pop a, b; push b^a |
| `0x10` | 1 | SHL | Pop a, b; push b<<a |
| `0x11` | 1 | SHR | Pop a, b; push b>>a (arithmetic) |
| `0x12` | 1 | LT | Pop a, b; push (b < a) |
| `0x13` | 1 | LE | Pop a, b; push (b ≤ a) |
| `0x14` | 1 | GE | Pop a, b; push (b ≥ a) |
| `0x15` | 1 | GT | Pop a, b; push (b > a) |
| `0x16` | 1 | EQ | Pop a, b; push (b == a) |
| `0x17` | 1 | NE | Pop a, b; push (b ≠ a) |
| `0x18` | 1 | LAND | Pop a, b; push (b≠0 && a≠0) |
| `0x19` | 1 | LOR | Pop a, b; push (b≠0 \|\| a≠0) |
| `0x1A` | 1 | ABS | Pop a; push abs(a) |
| `0x1B` | 1 | NEG | Pop a; push −a |
| `0x1C` | 1 | NOT | Pop a; push (a == 0) |
| `0x1D` | 1 | RANDOM | Pop N (0–65535); push random(0..N−1) via engine RNG |
| `0x1E` | 1 | PERCENT | Pop N; push (random_100 < N) |
| `0x1F` | 1 | CHANCE | Pop N; scale by skill level (÷100 per level > 2); push (random_100 < scaled_N) |
| `0x20` | 3 | GOTO | Read s16 offset; jump to base + offset |
| `0x21` | 3 | PUSH_GOTO | Push (IP+1 − base), then execute GOTO with following s16 offset |
| `0x22` | 1 | JUMP | Pop addr; jump to base + addr |
| `0x23` | 3 | IF_FALSE | Pop cond; if cond==0: jump to base + s16 offset; else skip 2 bytes |
| `0x24` | varies | SWITCH | Pop idx; if 0 ≤ idx < N: jump to indexed table (1+N×2 bytes); else skip table |
| `0x26` | 5 | CALL_DIRECT | IP += 5; call *(code**)(IP+1); push return value |
| `0x27` | varies | CALL_BY_NAME | Look up null-terminated name, call; push return value; **self-patches to CALL_DIRECT** for subsequent calls (JIT optimization) |
| `0x28` | 5 | FRAME | Read 2 s16 values into `DAT_00546c44`/`DAT_00546c46`; IP += 4 |

**Evaluation stack & argument readers:**

`FUN_00465ad0` (0x465ad0) is the raw stack-pop function — pops one dword from the 32-entry eval stack at `DAT_00546bf2[DAT_00546c42 - 1]`. The CTDo_ handlers pop their arguments by calling higher-level wrappers that additionally validate and convert units:

| Address | Name (inferred) | Return value | Converts from |
|---------|----------------|--------------|---------------|
| `FUN_00465ad0` | `read_raw` | raw dword from eval stack | raw value |
| `FUN_00465d40` | `read_heading` | normalized heading in binary degrees (0–359° × 182) | normalizes to [0, 359], then × 182 |
| `FUN_00465c90` | `read_angle` | angle in binary degrees (clamped ±90° × 182) | clamps to [−90, 90], then × 182 |
| `FUN_00465da0` | `read_alt` | altitude in binary degrees (clamped ±180° × 182) or `0x7FFFFFFF` (`any`) | clamps to [−180, 180], then × 182; passthrough if = 0x7FFFFFFF |
| `FUN_00465de0` | `read_duration` | unsigned int 0–15 (capped) | clamps to [0, 15] |
| `FUN_00465e00` | `read_speed` | speed in binary degrees, clamped to aircraft [min_speed, max_speed] | reads aircraft speed bounds at runtime |

**CTDo_move (`FUN_00465cc0`) — confirmed arg sequence:**

Calls `FUN_004ac510(heading, alt, roll_or_any, alt_is_any, vel_x, vel_y, speed, duration)`:
1. `heading` (binary degrees, 0–359° normalized) — from `read_heading`
2. `alt` (binary degrees, ±90°) — from `read_angle`
3. `roll` (binary degrees, ±180°, or `0x7FFFFFFF` = `any`) — from `read_alt`
4. `alt_is_any` (bool) — derived from altitude arg being 0x7FFFFFFF (the `any` sentinel)
5–6. velocity carry-over from previous command (`_DAT_00546c9c`, `_DAT_00546ca0`, zeroed after use)
7. `speed` (binary degrees, clamped to aircraft speed range) — from `read_speed`
8. `duration` (0–15 ticks) — from `read_duration`

`FUN_004ac510` (_MVRMove): clamps `alt` to ±0x3FFC (±90°); when `alt_is_any` = true → maneuver type 6 (any altitude) / roll target = 0; when false → type 1, roll target = `roll` arg.

**CTDo_turn (`FUN_00465ea0`) — confirmed arg sequence:**
1. min heading (degrees, clamped to current turn rate via `FUN_004780d0`)
2. max heading (clamped similarly)
3. type/mode (5 = timed, 6 = `any`-time/unconditional)
4. target heading in binary degrees (`arg * 182`, i.e. `arg * 65536/360`)
5. ctrl
6. duration

| BI file | Size | AI source size |
|---------|------|----------------|
| AC130.BI, B.BI, HYDRO.BI, LARGE.BI, LINER.BI | 4,608 B | 960–3,970 B |
| H.BI | 8,704 B | 12,412 B |
| F.BI, F117.BI, MOTH.BI | 12,800 B | 18,423–20,616 B |

BI sizes are smaller than the source text because the compiled DLL uses a compact bytecode representation. The compiler is internal to FA's toolchain and not distributed.

## TODO — Deep Dive

- Opcode 0x28 (`FRAME`): purpose of `DAT_00546c44`/`DAT_00546c46` not yet confirmed — binary survey of all 9 `.BI` files shows every dispatch entry and handler label starts with a FRAME; first s16 is a sequential per-block ID (IDs 1–6 compiler-reserved; all `.BI` files start their dispatch chain at ID 7); second s16 increases monotonically and is not a valid code pointer (not read by any function in the scanned interpreter range — may be used by a profiling or priority subsystem outside the Ghidra scan scope)
- Confirm `<speed_mode>` and `<value>` argument names for `move` in the AI source: the bytecode arg readers pop heading, angle, alt/roll, speed, duration — map these to the AI source syntax precisely
- `_MVRJink@40` confirmed at 0x4ac9e0 (force-created): jink executes a loop of `FUN_00463a20` calls alternating heading ± angle; jink arg semantics (`param_8` = count, `param_9` = ctrl, `param_10` = duration, `param_3`/`param_4` = deflection angles) now decompiled but full semantic labels need live confirmation

## Related

- [BI.md](BI.md) — compiled binary companion, one per `.AI` file
- [BRF.md](BRF.md) — object type records (`.OT`, `.NT`, `.PT`) that reference AI files by name
