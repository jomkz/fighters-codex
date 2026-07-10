---
format: EFFECT
name: GRAPHIC effect-spawn data
extensions: []
family: EFFECT
category: system
endianness: little
spec:
  status: partial
  gaps:
    - kind: re-static
      issue: 54
      note: "several bytes of the 0x30-byte effect-parameter record (incl. +0x00..+0x03) are not individually resolved; the sound-name pointer slots hold absolute VAs that need the whole image to dereference"
codec:
  direction: read
  rationale: "the effect-parameter table and the MSG 0x8003 spawn record live inside the game executable and its network stream — there is no codex-authored file the engine loads, so the interpreter decodes a supplied buffer into a semantic form and there is nothing to serialize back (round-trip decision, #101)"
  lib: [lib/src/effect.cpp]
  commands: [effect]
  tests: [tests/test_effect.cpp]
  fuzz: [fuzz/fuzz_effect.cpp]
  fixtures:
    synthetic: true
    real_manifest: false
related: [SH, OT]
---

# EFFECT — GRAPHIC effect-spawn data

The transient visual effects the game spawns — explosions, smoke, fire,
debris, craters, chaff/flare, dust puffs — are driven by two small binary
records. Neither is an on-disk file: they live inside the game executable and
its network stream. This page specifies their byte layout so tooling can turn
them into a semantic form; the full runtime that consumes them is documented,
from the engine side, in [objects.md](../objects.md) § *GRAPHIC effect
spawning*, and the effects reference [SH](SH.md) shapes.

`fx_lib` provides a read-only interpreter (`lib/src/effect.cpp`,
[api.md](../../api.md) § `effect.h`) — the validation lens for this spec, in the
same relationship the SH interpreter has to [SH.md](SH.md). It never transcribes
game bytes: callers supply a buffer (a synthetic fixture, or the real table
sliced from the executable's `.data` at integration time).

## Tools

### fx

```
fx effect types                  # list effect type -> class / .SH shape
fx effect dump  <table.bin> [-n N] # decode N 0x30-byte parameter records
fx effect spawn <record.bin>     # decode a 17-byte MSG 0x8003 spawn record
```

`fx effect types` needs no game data — it prints the type→shape classification
alone. `dump` and `spawn` decode a raw buffer.

## File Layout

### Effect-parameter record (`0x30` bytes, indexed by type)

The engine holds one fixed **`0x30` (48)-byte** tuning record per effect type,
in a table at `FA.EXE` `0x4f46c4`; effect type `t` is `table + t*0x30`. Read by
`_GRAPHICAddExp@28` (`0x4432d0`). Confirmed fields:

| Offset | Type | Field | Meaning |
|--------|------|-------|---------|
| `0x04` | s16 | `intensity` | base brightness / scale (further scaled by a random factor at spawn) |
| `0x06` | s16 | `frame_count` | shape frame count / start frame |
| `0x08` | u16 | `subtype` | sub-type / shape selector; low byte **bit 2** = ground burst |
| `0x0A` | s16 | `debris_count` | secondary-debris count |
| `0x0C` | s16 | `debris_spread` | secondary-debris spread |
| `0x0E`.. | u32[≤8] | `sound_ptrs` | sound-effect name pointers (absolute VAs); one picked at random per spawn, list ends at the first null |
| `0x2E` | s16 | `sound_pitch` | sound pitch / parameter |

Bytes `+0x00`–`+0x03` and the remainder of the record are not yet individually
resolved (see Open Questions). The interpreter reports the count of populated
(non-null) `sound_ptrs` slots as `sound_variants`; the names themselves cannot
be resolved from the record alone.

### Effect type → shape

`_GRAPHICInit@0` (`0x442c00`) fills a per-type `.SH` handle table; the type
index therefore classifies the effect:

| Type(s) | Class | Shape |
|---------|-------|-------|
| `0` | none | — |
| `1`–`3` | crater | `crater.SH` |
| `4`–`6` | debris | `debris.SH` |
| `7`–`11` | smoke | `smoke.SH` |
| `12` | chaff | `chaff.SH` |
| `13` | flare | `flare.SH` |
| `14` | fire | `fire.SH` |
| `15`–`0x26` | explosion | `exp.SH` |
| `0x28`–`0x2A` | dust-puff | `spd.SH` / `mpd.SH` / `lpd.SH` |

`0x27` and anything past `0x2A` are unclassified.

### Network spawn record (`0x11` bytes — `MSG 0x8003`)

When a machine spawns an effect it mirrors it to peers as a 17-byte message,
drained remotely by `ProcessEffectMsgs` ([objects.md](../objects.md)):

| Offset | Type | Field |
|--------|------|-------|
| `0x00` | u8 | effect `type` |
| `0x01` | s32 | `x` (F24.8 world position, feet = raw/256) |
| `0x05` | s32 | `y` |
| `0x09` | s32 | `z` |
| `0x0D` | u16 | `owner` object index (`0xffff` = none) |
| `0x0F` | u8 | `flag0` |
| `0x10` | u8 | `flag1` |

## Engine Notes

The static record is only the *seed*: at spawn time `_GRAPHICAddExp@28` applies
a random per-type variation (a clear-sky explosion may promote to one of several
concrete variants), scales `intensity` by a random factor, and chains secondary
debris / cluster-release / smoke children. The spawned effect then lives in the
100-entry GRAPHIC pool (`0x66`-byte entries) and is integrated each frame by
`_GRAPHICUpdate@0`. All of that runtime behaviour — the pool entry layout, the
adder (continuous-emitter) mechanism, and the fuse/lifecycle — is documented in
[objects.md](../objects.md) § *GRAPHIC effect spawning*. This spec covers only
the two data records a tool can decode.

## Open Questions

### 1. Unresolved record bytes (#54)

The leading bytes `+0x00`–`+0x03` of the `0x30` record and the tail beyond
`0x2E` are not individually mapped; only the fields above are confirmed from the
`_GRAPHICAddExp@28` decompile. The `sound_ptrs` slots hold absolute VAs into the
executable's string data, so resolving them to sound-effect names needs the full
image, not the record in isolation. Tracked under [#54](https://github.com/jomkz/fighters-codex/issues/54).

## Related

**Engine:** [objects.md](../objects.md) § GRAPHIC effect spawning — the runtime
pool, spawn API, and lifecycle. **Shapes:** [SH](SH.md) — the `.SH` shapes each
effect type draws. **Owners:** [OT](OT.md) — the object types effects attach to.
