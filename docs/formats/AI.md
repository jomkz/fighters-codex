# Artificial Intelligence -- Object AI (.AI)

FA_2.LIB contains 9 `.AI` files — one per object class (e.g. `AC130.AI`, `BOMBER.AI`). Each defines the AI behaviour for that class using a custom goto-based scripting language parsed as **plain ASCII text**.

## Format

Plain ASCII, CRLF line endings. A companion compiled binary `.BI` file exists for each `.AI` script.

### Structure

```
; comment lines begin with semicolon
if <condition> goto <label>
...
exit

<label>
  <instructions>
```

The engine evaluates the `if` chain from the top on each AI tick, jumping to the first matching condition's label. `exit` falls through when no condition is met.

### Example — AC130.AI (excerpt)

```
;*********************************************************
; ac130.ai — Custom AI for AC-130 Spectre gunship.
; Similar to bomber AI but uses pylon left turn for attack.
;*********************************************************
if do_nothing goto nothing
if do_ir_launch goto ir_launch
if do_radar_launch goto radar_launch
if do_hit goto hit
if do_evade goto evade
if do_attack goto attack
exit
```

## Location

| LIB | Count |
|-----|-------|
| FA_2.LIB | 9 |

## TODO — Deep Dive

- Enumerate all valid condition keywords and instruction opcodes across all 9 files
- Locate the script parser/interpreter in FA.EXE (xref to `.AI` filename loading)
- Confirm relationship to `.BI` (pre-compiled cache vs. independent data)

## Related

- [BI.md](BI.md) — compiled binary companion, same entry count
- [BRF.md](BRF.md) — object type records (`.OT`, `.NT`, `.PT`) that reference these files by name
