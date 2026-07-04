# Object / Entity System

How FA.EXE stores, services, and dispatches behavior for every live game object —
aircraft, ground vehicles, projectiles, and static props alike. This is the runtime
spine the flight model, AI, weapons, and renderer all hang off: one **object chain**
walked every frame, a **current-object mirror** each handler operates on, and a
**proc-dispatch** indirection that routes events, damage, and updates to the right
per-class code.

> **Provenance:** Ghidra static analysis of FA.EXE with [FA.SMS](formats/SMS.md)
> symbols applied; recovered from `DumpAllFunctions.txt` and `AnalyzeOTNT.txt`
> ([scripts/ghidra/](https://github.com/jomkz/fighters-codex/tree/main/scripts/ghidra)).
> Every symbol here is recorded in the
> [symbol database](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/objects.csv)
> and applied to the Ghidra project; progress is tracked in the
> [reconstruction matrix](reconstruction.md). Confidence markers follow
> [spec-authoring.md](../spec-authoring.md): confirmed · inferred · unknown.

## Objects, ids, and the pointer table

Every object is a variable-length record addressed by a small integer **id**. The
global pointer table `_objPtrs` (`0x553848`) maps `id → record base`; almost all
engine code reaches an object as `(&_objPtrs)[id]`. Id `0` is the null object. Ids
are handed out by the allocator (below); a separate high band of **alias ids**
(negative, per-computer) is reserved for multiplayer references.

The first bytes of every record are shared across all classes — the fields the
object system itself reads:

| Offset | Size | Field | Meaning | Confidence |
|--------|------|-------|---------|------------|
| `+0x00` | 1 | `class` | object class tag (`& 0x1f`); `4` = aircraft | confirmed |
| `+0x01` | 4 | `flags` | status bits; `& 1` = alive, `& 0x100000` = draw destroyed model | confirmed |
| `+0x05` | 4 | `type` | pointer to the shared **type record** (OT/NT/PT/JT) | confirmed |
| `+0x0E` | 2 | `health` | `0` = destroyed | confirmed |
| `+0x11` | 12 | `pos` | world position (X,Y,Z, 24.8 fixed) | confirmed |
| `+0x64` | 2 | `next_id` | next object in its service chain | confirmed |
| `+0x68` | 2 | `service_key` | sort key that orders the service chain | inferred |
| `+0x6C` | 4 | `event_override` | optional per-instance proc override | inferred |

The per-instance record is documented in full in [structs.md](structs.md); the
shared **type record** (and its shape fields) in
[shape-selection.md](shape-selection.md).

## The current-object mirror (`_cg` / `_cgt`)

Handlers do **not** operate on the object record in place. `GetCurObj` (`0x4628B0`)
copies the whole record into a fixed scratch buffer `_cg` (`0x50CE80`) and its type
record into `_cgt` (`0x50D268`), records the id in `_curId` (`0x4F6FBC`), and (for
aircraft) unpacks the flight-model fields. Every subsystem then reads and writes
`_cg`/`_cgt` — which is why so much of the engine references fixed addresses like
`_cg+0x11` (position) rather than an object pointer. `PutCurObj` (`0x462980`) copies
the mirror back and clears `_curId`. `_curObjSize`/`_curTypeSize` hold the byte
counts to copy; `PushCurObj`/`PopCurObj` nest the mirror (`_curObjStackTop`) so a
handler can service a second object re-entrantly.

This mirror is the reason the object record layout and the `_cg` global layout are
the same map — a fact this subsystem's waivers record explicitly, pointing every
`_cg+N` interior back to entity `+N` in [structs.md](structs.md).

## The service chain and per-frame loop

Active objects live on a singly linked **service chain** (`_chainStart`, `0x546B90`),
ordered by `service_key` (`+0x68`). `ServiceObjects` (`0x462A50`) walks it once per
frame:

1. `GetCurObj` mirrors the head object.
2. If its ready time is in the future, stop (the chain is time-ordered).
3. Detach the head, run its update via `Service` → the proc dispatch below.
4. If the handler destroyed the object and the type opts into auto-removal
   (`type +0x09 & 0x400`), `RemoveCurObj` unlinks it.
5. `PutCurObj` writes the mirror back, then the object is re-queued.

Servicing pulls objects off `_chainStart` and collects the ones to run again into a
**re-queue chain** (`_requeueChain`, `0x546BA8`); `ChainMergeSorted` (`0x462B70`)
folds that back into `_chainStart` in `service_key` order at end of frame.
`ChainInsertCurObj` (`0x4626D0`) does the ordered insert; `ChainRemoveCurObj`
(`0x462640`) the unlink. After the walk, `ServiceObjects` drains two remote-event
queues (below).

![Object lifecycle: allocation, the per-frame service loop over the object chain, the current-object mirror, and proc dispatch to per-class handlers.](diagrams/objects-lifecycle.svg)

## Proc dispatch — one object, many behaviors

An object's behavior is not called directly; it is resolved through a **proc
selector**. `CallUtilProc` (`0x463F60`) is the hub: given a selector index it calls
`GetObjProc` (`0x463F30`), which returns either the object's per-instance override
(`+0x6C`) or the class proc looked up on the type record (`+0x7D`), then invokes it.
Higher-level entry points wrap it:

- `CallEventProc` (`0x4639C0`) — routes a game event, first offering it to a global
  `_eventFilterProc` hook (used by the mission/AI layer to intercept).
- `CallDamageProc` (`0x463EC0`) — routes a hit; may trigger `RemoveCurObj` when the
  object dies and the type auto-removes.

Each object **class** publishes its procs through a small selector function.
`OBJProc` (`0x473BE0`) is the static-object one: selector `3` → `OBJEventProc`
(`0x473A40`), `4` → `OBJDamageProc` (`0x473B40`), `6` → `OBJSayProc`. Aircraft,
projectiles, and ground vehicles (`GV`) publish their own proc sets the same way, so
the same `CallUtilProc` call reaches class-appropriate code.

## Allocation and aliases

Objects are bump-allocated from a single arena. `OBJInit` (`0x491250`) reserves the
arena (`_objArena`, capacity `_objArenaSize`) and seeds the id counter and the
per-computer alias band (`_tempAliasBase`/`_tempAliasMax`). `OBJAdd` (`0x4913E0`)
copies a prepared record to the arena bump cursor (`_objArenaNext`), records its byte
size in `_objSizes`, and publishes the `id → base` entry in `_objPtrs`; `OBJSubtract`
(`0x491490`) pops the most recent one. `OBJAlias` (`0x4914C0`) and its variants map a
transient reference (waypoint, multiplayer peer, preferred target) onto a real id so
that AI and networking can name objects that may not be locally resident.

## Remote effect and hit queues

In multiplayer, other computers' hits and effects arrive as messages, drained after
the service walk:

- `ProcessHitMsgs` (`0x462C91`) — reads hit events (`MSG 0x800B`/`0x800C`), raises
  the `0x4000` event on the target, and spawns the explosion via `GRAPHICAddExp`.
- `ProcessEffectMsgs` (`0x462D40`) — reads per-computer effect spawns
  (`MSG 0x8003+n`): explosions, smoke trails, and `MANAdd` man/parachute spawns.

Local destruction takes the same visual path from the flight model:
`PLANEBreakUp` sets the `0x300000` destroyed/awaiting-swap flags and writes the
damage-set selector — see [shape-selection.md](shape-selection.md).

## Globals

Recovered object-system state (full list, with per-symbol confidence, in the
[symbol database](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/objects.csv)):

| Global | Address | Role | Confidence |
|--------|---------|------|------------|
| `_objPtrs` | `0x553848` | `id → record base` pointer table | confirmed |
| `_chainStart` | `0x546B90` | head of the service chain | confirmed |
| `_requeueChain` | `0x546BA8` | objects to re-queue this frame | confirmed |
| `_curId` | `0x4F6FBC` | id of the mirrored current object | confirmed |
| `_cg` | `0x50CE80` | current-object record mirror | confirmed |
| `_cgt` | `0x50D268` | current-object type-record mirror | confirmed |
| `_curObjSize` | `0x546B94` | bytes to copy for the object mirror | confirmed |
| `_curTypeSize` | `0x546B9C` | bytes to copy for the type mirror | confirmed |
| `_objArena` | `0x4FFE34` | base of the entity arena | confirmed |
| `_objArenaNext` | `0x553828` | arena bump cursor | confirmed |
| `_objArenaSize` | `0x553840` | arena capacity, bounds `OBJAdd` | confirmed |
| `_objSizes` | `0x553120` | per-id record byte sizes | confirmed |
| `_nextObjId` | `0x553838` | next id to allocate | confirmed |
| `_tempAliasNext` | `0x55383C` | next transient alias id | confirmed |

## Functions

| VA | Symbol | Role |
|----|--------|------|
| `0x00462600` | `InitChain` | reset the service chain and event hook |
| `0x00462A50` | `ServiceObjects` | per-frame walk of the object chain |
| `0x004626D0` | `ChainInsertCurObj` | ordered insert by `service_key` |
| `0x00462640` | `ChainRemoveCurObj` | unlink the current object from a chain |
| `0x00462B70` | `ChainMergeSorted` | fold the re-queue chain back into `_chainStart` |
| `0x004628B0` | `GetCurObj` | copy an object + type into the `_cg`/`_cgt` mirror |
| `0x00462980` | `PutCurObj` | write the mirror back to the record |
| `0x00463F60` | `CallUtilProc` | resolve and call a proc by selector |
| `0x00463F30` | `GetObjProc` | pick instance override (`+0x6C`) or class proc (`+0x7D`) |
| `0x004639C0` | `CallEventProc` | route a game event (through `_eventFilterProc`) |
| `0x00463EC0` | `CallDamageProc` | route a hit; may auto-remove on death |
| `0x00473BE0` | `OBJProc` | static-object proc selector |
| `0x00473A40` | `OBJEventProc` | static-object event handler |
| `0x00473B40` | `OBJDamageProc` | static-object damage handler |
| `0x00491250` | `OBJInit` | reserve the entity arena and id/alias bands |
| `0x004913E0` | `OBJAdd` | append a record; publish `id → base` |
| `0x00491490` | `OBJSubtract` | pop the most recently added record |
| `0x004914C0` | `OBJAlias` | map a transient reference onto a real id |
| `0x00462C91` | `ProcessHitMsgs` | drain remote hit events → explosions |
| `0x00462D40` | `ProcessEffectMsgs` | drain remote effect spawns |
| `0x00436B30` | `MoveObj` | advance an object toward its move goals |
| `0x004A6EB0` | `SetupOT` | type-load: generate damage-model variants (see shape-selection) |

## Open questions

### 1. `service_key` (`+0x68`) time base

The chain is ordered by `+0x68` and `ServiceObjects` stops at the first object whose
key is in the future, so `+0x68` is a scheduling tick. Whether it is an absolute
frame tick or an offset from `_currentTicks` (and its wrap behavior near `0x7FFF`,
which `TimeAddSat` guards) is not yet pinned — a short trace of the writers of
`+0x68` will settle it.

*Status: open — re-static.*

## Related

- [shape-selection.md](shape-selection.md) — the whole-model damage swap and the
  type-record shape fields this system's objects carry.
- [structs.md](structs.md) — the full per-instance entity record mirrored into `_cg`.
- [game-loop.md](game-loop.md) — where `ServiceObjects` sits in the per-frame update.
- [physics.md](physics.md) — the flight model that runs inside an aircraft's service
  slot and drives `PLANEBreakUp`.
- [network.md](network.md) — the multiplayer layer that produces the remote hit and
  effect messages, and consumes object aliases.
