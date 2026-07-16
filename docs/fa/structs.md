# FA Struct Reference

Master struct reference for Jane's Fighters Anthology.

> **Provenance:** Ghidra static analysis of the game executable with [FA.SMS](formats/SMS.md) symbols applied. The **recovered layouts** (§1, §2) are derived from the engine's own code and are asserted in [`db/types/fa_types.h`](https://github.com/jomkz/fighters-codex/blob/main/db/types/fa_types.h); the **field census** (§5 onward) is raw `RecoverStructs.java` output kept as leads. Confidence markers follow [spec-authoring.md](../spec-authoring.md): confirmed · inferred · unknown.

## The object model — two families, both self-describing

The engine has **two** record families, and most of the confusion in earlier revisions of
this document came from treating them as one:

| | what it is | where it lives | mirrored at |
|---|---|---|---|
| **`entity`** | the per-instance **object** record | bump arena, addressed by id via `_objPtrs` | `_cg` (`0x50CE80`) |
| **`OBJ_TYPE`** | the shared **type** record the object's `+0x05` points at | loaded from the `.OT` / `.NT` / `.PT` / `.JT` files | `_cgt` (`0x50D268`) |

The `*_TYPE` structs (`PROJ_TYPE`, `PT_TYPE`, `GV_TYPE`, `OT_TYPE`, `NT_TYPE`) are **per-class
extensions of the type record** — they are type data, *not* overlays of an entity allocation.

**Both records are variable-size, and each declares its own size:**

```
object bytes = 0xDE + type->obj_ext_size      (then rounded up to a dword)
type   bytes = type->type_size
```

This is confirmed twice, independently. `OBJAdd` (`0x4913E0`) has exactly one call site — in
`_T_AddObj` (`0x4A73B0`) — which asks for `type[+0x03] + 0xDE` bytes and `memmove`s them out of
`_cg`; and `GetCurObj` (`0x4628B0`) computes `_curObjSize` the same way before mirroring a record
back into `_cg`. `OBJAdd` records each record's byte length in `_objSizes[id]`.

So **there is no `sizeof(entity)` for a whole object**: `0xDE` is the *common region* every class
shares, and the class extension begins immediately after it.

### The classes, and how big each extension is

The extension's length is declared per type at `OBJ_TYPE +0x03` — and **the retail data states it
outright.** The type files are `[brent's_relocatable_format]`: *text*, which delimits the record
with the developers' own section names. Read from all **534** type records shipped in the game's
LIB archives:

| file | sections in the record | `utilProc` (the class) | object extension |
|---|---|---|---|
| `.OT` | `OBJ_TYPE` | `_OBJProc` ×157, `_STRIPProc` ×13 | **0 — all 170 records** |
| `.JT` | `OBJ_TYPE` + `PROJ_TYPE` | `_PROJProc` ×135 | **52 — all 135 records** |
| `.NT` | `OBJ_TYPE` + `NPC_TYPE` | `_GVProc` ×73, `_CARRIERProc`, `_EJECTProc`, `_CATGUYProc` | 145–247 |
| `.PT` | `OBJ_TYPE` + `NPC_TYPE` + `PLANE_TYPE` | `_PLANEProc` ×145 | 490–626 |

So a **static object has no extension at all** — its record is exactly `0xDE` bytes. And `OBJ_TYPE`
turns out not to be our coinage: it is the game's own name for the header.

The class procs also declare a **hierarchy**, by falling back to one another for selectors they do
not handle:

```
PLANE → GV → OBJ        CARRIER / EJECT / CATGUY → OBJ        PROJ, STRIP → (none)
```

### Why the low band still cannot be attributed by address

Every class's extension starts at `0xDE`, so the first 52 bytes are *simultaneously* a projectile's
whole extension, an aircraft's first 52 and a ground vehicle's first 52. An access to `_cg + 0xDE`
names no class at all.

**Two things do attribute a field:**

1. **Size.** A GV extension is at most 247 bytes and a projectile's is 52 — so a field at
   `ext ≥ 247` can *only* be an aircraft's. The shipped data proves it.
2. **A class guard in the code.** `ShapeSetup` reads its fields only after testing
   `entity.obj_class == 4` (aircraft), which proves the class of what it then reads.

**And one thing does not: call-graph reachability from the class procs.** It looks sound and is not
— a handler routinely mirrors *another* object into `_cg` (`GetCurObj` on a target id), so "reached
from `_PROJProc`" does not mean "a projectile's field". Measured, it attributes fields at `ext 0x99`
and `0x9D` to projectiles, whose extension is only 52 bytes long. Recorded so it is not retried.

### The `.SH` articulation runs through this extension

The x86 embedded in a shape file never touches `_cg` — the engine **stages** the state for it.
`ShapeSetup` (`0x4AB450`) reads the aircraft extension and marshals it into the `_PL*` globals
(`_PLgearDown`, `_PLgearPos`, `_PLhook`, `_PLrightFlap`, `_PLafterBurner`, …), and the `.SH` x86
*selectors* read those to pick which sub-stream geometry to draw:

```
plane extension in _cg  →  ShapeSetup  →  _PL* globals  →  .SH embedded x86  →  sub-stream geometry
```

`PLANE_EXT::state_flags` (`+0x091`, i.e. `entity +0x16F`) carries the discrete state:
`0x20` afterburner · `0x40` gear down · `0x80` brake · `0x100` slats/flaps · `0x400` hook ·
`0x1000` computer flight. The **gear articulation** is not a bit at all — it is the s16
`gear_pos` (`entity +0x238`, sentinel `0x7FFF`), which ShapeSetup stages into `_PLgearPos`.
See [shape-selection.md](shape-selection.md) and [formats/SH.md](formats/SH.md).

### How the recovered fields were proven

The mirrors make the evidence unusually strong. Code that reaches a field through the current-object
mirror addresses it **absolutely** (`_cg+N`), so the instruction's operand size *proves* the field's
width, and the referencing function identifies its subsystem.

Note the asymmetry, which is what keeps this honest: **presence in that census proves a field exists
at that offset with that width; absence proves nothing** — code that reaches the same field through
a register pointer never appears. Hence §1 and §2 name only what the census and the prose docs
corroborate, and leave every other byte explicitly reserved.

All sizes are in bytes. Multi-byte integers are little-endian. Fixed-point values use FA's standard
F24.8 format (24-bit integer part, 8-bit fraction) unless noted.

## 1. `entity` — the object record (common region)

Every live game object (aircraft, missile, vehicle, static object, NPC…) begins with this
`0xDE`-byte common region; its class extension follows at `0xDE`. Allocated by `OBJAdd`, serviced
through the `_cg` mirror — see [objects.md](objects.md).

This is the **recovered layout**, asserted in
[`db/types/fa_types.h`](https://github.com/jomkz/fighters-codex/blob/main/db/types/fa_types.h)
(`static_assert(sizeof(entity) == 0xDE)`, which the `fxe` build evaluates on every compile). Bytes
not listed are reserved: not "empty", but *not proven*.

| Offset | Size | Field | Meaning | Confidence |
|--------|------|-------|---------|------------|
| `0x00` | 1 | `obj_class` | class tag (`& 0x1f`); `4` = aircraft | confirmed |
| `0x01` | 4 | `obj_flags` | `& 1` alive; `& 0x100000` draw destroyed model | confirmed |
| `0x05` | 4 | `obj_type` | → the `OBJ_TYPE` record (§2) | confirmed |
| `0x09` | 1 | `unk_09` | byte field; touched by 7 subsystems | inferred |
| `0x0A` | 4 | `unk_0A` | | inferred |
| `0x0E` | 2 | `obj_health` | `0` = destroyed | confirmed |
| `0x10` | 1 | `unk_10` | | inferred |
| `0x11` | 12 | `pos_x/y/z` | world position, F24.8 | confirmed |
| `0x1D` | 2 | `heading` | some code reads `0x1D` as a dword (heading+pitch together) | confirmed |
| `0x1F` | 2 | `pitch` | | confirmed |
| `0x21` | 2 | `bank` | third of the `heading`/`pitch`/`bank` orientation triple (`0x1D`/`0x1F`/`0x21`); the earlier `goal_angle` was a retracted access-note guess (`fa_types.h`, physics.md) | confirmed |
| `0x23` | 2 | `goal_heading` | | inferred |
| `0x25` | 2 | `goal_altitude` | | inferred |
| `0x34` | 4 | `speed` | F24.8 | confirmed |
| `0x38` | 1 | `view_target` | view-target mode + id | inferred |
| `0x64` | 2 | `chain_next_id` | next object in the service chain | confirmed |
| `0x68` | 2 | `service_key` | sort key ordering the service chain | inferred |
| `0x6C` | 4 | `event_override` | optional per-instance proc override | inferred |
| `0x80` | 94 | `net_state` | per-object network replication block — every access in the range is from the network subsystem; interior unmapped | inferred |
| `0xDE` | — | *(class extension)* | length = `type->obj_ext_size`; aliases across classes, so unmapped | confirmed |

Smaller unnamed-but-proven fields (`0x50`–`0x76`) are carried in `fa_types.h` as `unk_*` with their
proven widths.

---

## 2. `OBJ_TYPE` — the type record (common header)

The shared per-class record loaded from the `.OT` / `.NT` / `.PT` / `.JT` files, pointed at by every
object's `+0x05` and mirrored at `_cgt`. Its **total** size is self-declared at `+0x01`; the header
below is the part generic object code reads on any class. (`OBJ_TYPE` is our name — the game's own
name for the common header is not recovered; the class extensions keep their FA.SMS names.)

| Offset | Size | Field | Meaning | Confidence |
|--------|------|-------|---------|------------|
| `0x01` | 2 | `type_size` | total bytes of this type record | confirmed |
| `0x03` | 2 | `obj_ext_size` | bytes of class extension on the *object* record (`object = 0xDE + this`) | confirmed |
| `0x09` | 4 | `type_flags` | `& 0x400` = auto-remove on death | confirmed |
| `0x0D` | 2 | `obj_class` | class bitfield; high byte `& 0xC0` gates the `_b` shape slot | confirmed |
| `0x0F` | 4 | `shape` | base shape | confirmed |
| `0x13` | 4 | `shape_name` | filename used as the suffix template | confirmed |
| `0x17` / `0x1B` | 4 | `shape_a` / `shape_b` | destroyed set `{A,B}` — world / graphics pass | confirmed |
| `0x25` / `0x29` | 4 | `shape_c` / `shape_d` | destroyed set `{C,D}`, aircraft only | confirmed |
| `0x33` | 4 | `damage_set` | `== 2` selects the `{_C,_D}` set | confirmed |
| `0x7D` | 4 | `class_proc` | the class's proc selector (`GetObjProc`) | confirmed |

The shape fields are documented in full in [shape-selection.md](shape-selection.md).

---

## 3. The class extensions — `PLANE_EXT`, `PROJ_EXT` (#476)

The extension begins at `entity + 0xDE`. Offsets below are **relative to the extension**; the
`_cg` column gives the absolute mirror address the code actually uses, which is how these were
recovered.

### `PLANE_EXT` — the aircraft extension

Every aircraft type in the retail data ships **at least 490 bytes** of extension (the tail beyond
that varies per type), so `sizeof(PLANE_EXT) == 490` is a floor the compiler checks, not the whole
record. Fields at `ext ≥ 247` are aircraft-only by the size argument above; the rest are named only
where `ShapeSetup` reads them under an `obj_class == 4` guard.

| ext | `_cg` | Size | Field | Meaning | Confidence |
|--------|--------|------|-------|---------|------------|
| `+0x005` | `0x0E3` | 1 | `pl_state` | staged into `_PLstate` for the shape selectors | inferred |
| `+0x091` | `0x16F` | 4 | `state_flags` | `0x20` afterburner · `0x40` **gear down** (`FMGear`) · `0x80` brake · `0x100` slats/flaps · `0x400` hook · `0x1000` computer flight (autopilot) | confirmed |
| `+0x0BD` | `0x19B` | 4 | `g_load` | `0x100` = 1 G | inferred |
| `+0x110` | `0x1EE` | 4 | `throttle` | current, smoothed | confirmed |
| `+0x114` | `0x1F2` | 2 | `throttle_target` | commanded %, 0–100 | confirmed |
| `+0x118` | `0x1F6` | 4 | `climb_rate` | tested against `-0x2D00` in the flap gate | inferred |
| `+0x11E` | `0x1FC` | 4 | `fuel` | internal quantity | confirmed |
| `+0x12E` | `0x20C` | 1 | `stall_state` | 0 normal, 1 warning, 2 stall | confirmed |
| `+0x15A` | `0x238` | 2 | `gear_pos` | gear **articulation position** (`0x7FFF` = not articulating) — staged into `_PLgearPos`/`_PLgearInc` | confirmed |
| `+0x196` | `0x274` | 2 | `ab_expiry` | afterburner timer (vs `_currentT`) | confirmed |
| `+0x198` | `0x276` | 2 | `brake_expiry` | wheel-brake timer | confirmed |

Everything else is `reserved`. In particular `+0x006`–`+0x090` is the band that **aliases** the
projectile and GV extensions: nothing there can be attributed by address, so nothing there is named.

### `PROJ_EXT` — the projectile extension

**Exactly 52 bytes, in all 135 shipped `.JT` records** — so unlike the aircraft's, this size is not
a floor but the whole thing, and it is asserted. Its interior is deliberately **unmapped**: those
52 bytes alias the aircraft and GV extensions byte for byte, and no census can tell them apart.
An honest 52-byte hole beats ten invented fields.

---

## 4. `CN_INFO` — network configuration

Persisted to and loaded from `EA.CFG` / `NET.DAT` on disk. `CN_ReadConfig` reads `0xDDC` bytes after a 4-byte checksum header; `CN_WriteConfig` writes the same. The struct is version-stamped at `[0x00]` (version byte: `1`, `2`, or `3`). Version 3 is the current live format.

**Disk source:** `EA.CFG` or `NET.DAT`  
**Total size:** `0xDDC` bytes (body only; 4-byte checksum prepended on disk)

| Offset | Size | Field name (inferred) | Accessor functions | Notes |
|--------|------|-----------------------|--------------------|-------|
| `0x00` | 4 | `cn_version` | `CN_ReadConfig`, `NET_SlaveShutdown` | Config version. `3` = current format. **confirmed** |
| `0x01` | 1 | `cn_flags` | `NET_SlaveInit`, `NET_MasterStartGame` | General net flags byte. **confirmed** |
| `0x04` | 4 | `cn_session_id` | `NET_CancelPlayerList`, `netDialogAppIO`, `ip2long` | Session or IP identifier. **confirmed** |
| `0x06` | 2 | `cn_hud_msg_flags` | `_HUDMessage@4`, `HUDReprintMessages` | HUD message display flags. **confirmed** |
| `0x07` | 1 | `cn_master_reject` | `NET_MasterRejectPlayer` | Set when master rejects a player. **confirmed** |
| `0x08` | 4 | `cn_pkt_state` | `player_list_process_pkt`, `slave_process_pkt` | Packet processing state. **confirmed** |
| `0x0A` | 1 | `cn_warning` | `HUDSetWarning@8` | HUD warning flags from net. **confirmed** |
| `0x0B` | 1 | `cn_unk_0B` | `usnfmain`, `HUDSetWarning@8` | **inferred** |
| `0x0C` | 4 | `cn_unk_0C` | `NET_SlaveInit`, `NET_RequestPlayerList` | **inferred** |
| `0x10` | 4 | `cn_master_ptr` | `NET_MasterInit`, `NET_MasterShutdown` | Master session handle. **confirmed** |
| `0x14` | 4 | `cn_player_list` | `NET_RequestPlayerList`, `NET_MasterRejectPlayer` | Player list pointer or state. **confirmed** |
| `0x20` | 4 | `cn_hud_range` | `HUDDrawRangeInfo` | Range info for HUD display. **confirmed** |
| `0x24` | 4 | `cn_hud_heading` | `HUDDrawHeading` | Heading value for HUD. **confirmed** |
| `0x28` | 4 | `cn_hud_speed` | `HUDDrawSpeed` | Airspeed for HUD. **confirmed** |
| `0x30` | 4 | `cn_master_shutdown` | `NET_MasterShutdown` | Shutdown state flag. **confirmed** |
| `0x34` | 4 | `cn_hud_alt` | `HUDDrawAlt` | Altitude for HUD. **confirmed** |
| `0x38` | 4 | `cn_hud_data` | `HUDDrawWeaponInfo` | Weapon info HUD data. **confirmed** |
| `0x40` | 4 | `cn_hud_draw_data` | `_HUDDraw@4` | HUD draw buffer reference. **confirmed** |
| `0x44` | 4 | `cn_hud_init_ext` | `_HUDInit@0`, `FUN_004089a0` | **inferred** |
| `0x52` | 1 | `cn_slave_id` | `NET_SlaveInit` | Slave player index. **confirmed** |
| `0x54` | 4 | `cn_hud_nearest` | `HUDFindNearest@8` | Nearest contact for HUD lock. **confirmed** |
| `0x60` | 4 | `cn_hud_data2` | `HUDDrawRangeInfo`, `FUN_00409bfb` | **inferred** |
| `0x64` | 4 | `cn_net_slave_state` | `NET_SlaveInit`, `slave_process_pkt` | Full slave network state. **confirmed** |
| `0x68` | 4 | `cn_flight_key` | `FlightKey@4` | Flight key binding data. **confirmed** |
| `0x7C` | 4 | `cn_flying_loop` | `FlyingLoop` | Game loop counter or tick. **confirmed** |
| `0x80` | 4 | `cn_unk_80` | `usnfmain`, `FlyingLoop` | **inferred** |
| `0xA0` | 4 | `cn_hud_disrupt` | `_HUDDraw@4` | HUD disruption state. **confirmed** |
| `0xB4` | 1 | `cn_hud_extra` | `HUDSetStability` | HUD stability display. **confirmed** |
| `0xB6` | 2 | `cn_stability` | `HUDSetStability` | Stability value sent over net. **confirmed** |
| `0xC0` | 4 | `cn_ifm_time` | `IFMSetTime@@YAXD@Z` | IFM timer. **confirmed** |
| `0xC8` | 4 | `cn_damage` | `_DAMAGEDoHit@12` | Damage data for net sync. **confirmed** |
| `0xD8` | 4 | `cn_slave_fail` | `handle_slave_connection_failed` | Failure state for disconnection. **confirmed** |
| `0xDB0` | — | `cn_tcp_block` | `_NetSetFactoryTCP` | TCP factory defaults block; set by `_NetSetFactoryTCP__YAXPAUCN_INFO_TCP___Z`. **confirmed** |
| `0x8E4` | 1 | `cn_has_mac_str` | `CN_ReadConfig` | Non-zero if MAC address string present at this offset. **confirmed** |
| `0x8E4` | 8 | `cn_mac_str[8]` | `CN_ReadConfig`, `_atohb` | ASCII MAC address string, converted to binary at `0xDD0`. **confirmed** |
| `0x8EC` | 12 | `cn_ip_str[12]` | `CN_ReadConfig`, `_atohb` | ASCII IP address string, converted to binary at `0xDD4`. **confirmed** |
| `0xDD0` | 8 | `cn_mac_bin[8]` | `CN_ReadConfig` | Binary MAC address bytes (decoded from `0x8E4`). **confirmed** |
| `0xDD4` | 12 | `cn_ip_bin[12]` | `CN_ReadConfig` | Binary IP address bytes (decoded from `0x8EC`). **confirmed** |
| `0xDDA` | 1 | `cn_addr_valid` | `CN_ReadConfig` | Set to `1` when both MAC and IP decoded successfully. **confirmed** |
| `0xDDC` | — | *(end of struct)* | `CN_WriteConfig` | `fwrite(param_1, 0xDDC, 1, _File)` confirms total body size. |

---

## 5. The field census — leads, not layouts

Everything below this point is raw `RecoverStructs.java` output: it records every `[reg + constant]`
dereference where the register held a struct-pointer argument. It is a **census of accesses with
guessed names**, and it is retained because it is a useful lead list — not because it is a layout.

Three of its tables are **superseded**, and are kept only for provenance:

- **`entity` (below)** — its names conflict with the code where the two overlap (it reads `+0x04` as
  "the most-accessed dword" and puts `speed` at `0x32`, where `GetCurObj` provably reads the type
  pointer at `+0x05` and the object's speed sits at `0x34`). §1 supersedes it.
- **`OT_TYPE` and `NT_TYPE`** — these two tables list *identical* offsets with *identical*
  accessors, because both "scans" ran over `0x400000`–`0x540000`, i.e. the whole executable. They
  are the same generic-access noise attributed twice, not two recovered layouts. Neither is
  promotable as it stands.
- **`PROJ_TYPE`** — its offsets are into the **type record**, not "the upper region of an entity
  allocation": `_PROJSpeed@8` is called as `_PROJSpeed(_cgt, …)`. The *offsets* are real; the family
  they were filed under was wrong.

### Confidence in the census tables

These tags describe the census tables in this section only; the recovered layouts in §1–§4
carry their own confidence markers, earned from the code.

Every `inferred` (Low-confidence) field was reviewed against its accessor set (#130). A field is
promoted to `confirmed` only when a **dedicated named accessor** whose name identifies this
specific offset exists (the bar stated above). Where a field remains `inferred`, the reason is one
of three — this is the "why it can't be promoted from static analysis alone":

1. **Generic `FUN_*` accessors only.** The functions that touch the offset are still unnamed in the
   applied FA.SMS symbol set, so the field's role cannot be established statically. These are the
   bulk of the remaining Low tags and are deferred to the dynamic / Ghidra re-tracing pass under
   [#54](https://github.com/jomkz/fighters-codex/issues/54) — reading what each `FUN_*` does with
   the offset is what earns the promotion, and that cannot be done from the committed docs/DB.
2. **Named but generic accessors.** The offset is touched only by broad multi-field routines
   (`_explode`, `_MoveObj@0`, `_HUDInit@0`, `_DAMAGEInit@0`, `_ServicePlayer@0`, the `MP*` sync
   functions, high-fan-out helpers like `FUN_00406a5e`/`FUN_0042d050`, …). The accessor name does
   not isolate *this* offset's meaning, so the field name stays a context-derived guess. (Contrast
   `0x32 speed`/`0x34 heading`, `confirmed` because `HUDDrawSpeed`/`HUDDrawHeading` map to exactly
   that field, with `0x36 unk_36`/`0x3A unk_3A`, `inferred` although the same routine reads them.)
3. **Probable cross-struct contamination.** A few offsets are reached by methods of an unrelated
   class and are almost certainly not fields of the struct they were scanned under — see the
   flagged `GV_TYPE` rows `0x5C`/`0x78`/`0x7C` (`CDirDrawSurface::*`). The scan cannot separate two
   structs that share a pointer-argument convention (see [Methodology](#notes-on-methodology)).

Promotions made in this pass: `entity 0xE8` and `GV_TYPE 0x3F` (both gained a dedicated named
accessor); the three contamination rows above are annotated in place. All other Low tags fall under
categories 1–2 and carry the standing rationale here rather than a repeated per-row note.

**Scan range:** `0x400000`–`0x540000`, offsets `0x00`–`0x11E`

| Offset | Size | Field name (inferred) | Accessor functions | Notes |
|--------|------|-----------------------|--------------------|-------|
| `0x00` | 1 | `obj_type` | `_explode`, `NET_SlaveInit`, `NET_SlaveShutdown` | Object type/class byte. Highest access count (6434×). **confirmed** |
| `0x01` | 1 | `obj_flags` | `_explode`, `FUN_00401180`, `FUN_00401280` | Primary flags byte; 8288× accesses. **inferred** |
| `0x02` | 1 | `obj_flags2` | `_explode`, `FUN_00401280`, `FUN_00401430` | Secondary flags. **inferred** |
| `0x03` | 1 | `unk_03` | `_explode`, `NET_SlaveInit`, `player_list_process_pkt` | **inferred** |
| `0x04` | 4 | `obj_id` or `obj_ptr` | `_explode`, `FUN_00401180`, `FUN_00401280` | Most-accessed dword (10188×); likely object index/handle. **inferred** |
| `0x05` | 1 | `unk_05` | `FUN_00401a60`, `handle_slave_connection_failed` | **inferred** |
| `0x06` | 1 | `unk_06` | `_explode`, `player_list_process_pkt` | **inferred** |
| `0x07` | 1 | `unk_07` | `slave_process_pkt`, `HUDDrawHeading` | **inferred** |
| `0x08` | 4 | `unk_08` | `FUN_00401180`, `FUN_00401280`, `FUN_004014b0` | 5673× accesses. **inferred** |
| `0x09` | 1 | `squawk_code` | `FlyingLoop`, `HUDDrawHeading`, `HUDSquawk@0` | Read by `@HUDSquawk@0`; IFF/squawk identifier. **confirmed** |
| `0x0A` | 1 | `warning_flags` | `slave_process_pkt`, `HUDSetWarning@8`, `_HUDInit@0` | Written by `@HUDSetWarning@8`. **confirmed** |
| `0x0B` | 1 | `unk_0B` | `usnfmain`, `HUDSetWarning@8`, `_DAMAGEInit@0` | **inferred** |
| `0x0C` | 4 | `unk_0C` | `_explode`, `FUN_00401180`, `FUN_00401280` | 2938× accesses. **inferred** |
| `0x0D` | 1 | `unk_0D` | `usnfmain`, `_HUDInit@0`, `FUN_0040ebc0` | **inferred** |
| `0x0E` | 1 | `unk_0E` | `player_list_process_pkt`, `usnfmain`, `_HUDInit@0` | **inferred** |
| `0x0F` | 1 | `unk_0F` | `usnfmain`, `HUDDrawRangeInfo`, `FUN_00409760` | **inferred** |
| `0x10` | 4 | `unk_10` | `_explode`, `FUN_00401180`, `NET_SlaveInit` | 4911× accesses. **inferred** |
| `0x14` | 4 | `unk_14` | `_explode`, `FUN_00401180`, `FUN_00401280` | 3399× accesses. **inferred** |
| `0x16` | 2 | `altitude` | `usnfmain`, `HUDDrawAlt`, `FUN_004089a0` | Read by `?HUDDrawAlt@@YIXXZ`. **confirmed** |
| `0x17` | 1 | `speed_high` | `HUDDrawSpeed`, `_ServicePlayer@0`, `_MSGSend@32` | Read by `?HUDDrawSpeed@@YIXXZ`. Part of speed value. **confirmed** |
| `0x18` | 4 | `unk_18` | `_explode`, `FUN_00401180`, `NET_SlaveInit` | 2555× accesses. **inferred** |
| `0x1C` | 4 | `unk_1C` | `_explode`, `FUN_00401180`, `NET_RequestPlayerList` | 1952× accesses. **inferred** |
| `0x1F` | 1 | `hvel_component` | `usnfmain`, `HUDDrawHVel`, `HUDDrawRangeInfo` | Read by `?HUDDrawHVel@@YIXXZ`. **confirmed** |
| `0x20` | 4 | `unk_20` | `_explode`, `FUN_004014b0`, `handle_slave_connection_failed` | 2028× accesses. **inferred** |
| `0x21` | 1 | `damage_zone` | `_DAMAGEDoHit@12`, `_RotatedOffsetF24@20`, `MessagesToPlayer` | Written by `_DAMAGEDoHit@12`. **confirmed** |
| `0x22` | 2 | `hud_init_flags` | `_HUDInit@0`, `_HUDShutdown@0`, `FUN_004089a0` | Read/written by HUD init/shutdown. **confirmed** |
| `0x24` | 4 | `unk_24` | `_explode`, `FUN_00401180`, `NET_SlaveInit` | 1289× accesses. **inferred** |
| `0x27` | 1 | `zone_flags` | `_HUDInit@0`, `@ArmPlane@4`, `_Collision@56` | Read by zone/collision init. **inferred** |
| `0x28` | 4 | `unk_28` | `_explode`, `NET_SlaveInit`, `NET_RequestPlayerList` | 1190× accesses. **inferred** |
| `0x29` | 1 | `net_slave_id` | `NET_SlaveInit`, `slave_process_pkt`, `NET_MasterInit` | Accessed in all slave/master NET functions. **confirmed** |
| `0x2C` | 4 | `unk_2C` | `_explode`, `FUN_00401180`, `NET_SlaveInit` | 885× accesses. **inferred** |
| `0x30` | 4 | `unk_30` | `FUN_00401180`, `handle_slave_connection_failed`, `_HUDInit@0` | 844× accesses. **inferred** |
| `0x32` | 2 | `speed` | `HUDDrawSpeed`, `FUN_00409f30`, `FUN_0040b320` | Read repeatedly by `?HUDDrawSpeed@@YIXXZ`. **confirmed** |
| `0x34` | 4 | `heading` | `slave_process_pkt`, `_HUDInit@0`, `HUDDrawHeading` | Read by `?HUDDrawHeading@@YIXXZ`. **confirmed** |
| `0x36` | 2 | `unk_36` | `HUDDrawSpeed`, `FUN_004089a0` | **inferred** |
| `0x38` | 4 | `unk_38` | `player_list_process_pkt`, `slave_process_pkt`, `FUN_004024d0` | 538× accesses. **inferred** |
| `0x3A` | 2 | `unk_3A` | `HUDDrawSpeed`, `FUN_004089a0` | **inferred** |
| `0x3C` | 4 | `unk_3C` | `FUN_004024d0`, `_HUDInit@0`, `HUDDrawSpeed` | 532× accesses. **inferred** |
| `0x3F` | 1 | `chat_line` | `_CHATGetEditLine@0`, `_MSGSend@32` | Read by `_CHATGetEditLine@0`. **confirmed** |
| `0x40` | 4 | `unk_40` | `_explode`, `usnfmain`, `_HUDInit@0` | 644× accesses. **inferred** |
| `0x44` | 4 | `unk_44` | `FlyingLoop`, `FUN_00406a5e`, `FUN_00409760` | 430× accesses. **inferred** |
| `0x48` | 4 | `unk_48` | `FUN_00402640`, `FUN_00406a5e`, `FUN_0040991b` | 233× accesses. **inferred** |
| `0x49` | 1 | `explosion_type` | `FUN_0043a5c0`, `_GRAPHICAddExp@28`, `_GRAPHICAddSmoke@28` | Written when creating explosion graphic. **confirmed** |
| `0x4B` | 1 | `tv_key_state` | `FUN_0040f2d0`, `_DAMAGEDoHit@12`, `TVKey@@YIGG@Z` | Read by `?TVKey@@YIGG@Z`. **confirmed** |
| `0x4C` | 4 | `unk_4C` | `FUN_00406a5e`, `FUN_00409760`, `FUN_00409bfb` | 252× accesses. **inferred** |
| `0x50` | 4 | `unk_50` | `FUN_00406a5e`, `FUN_00409760`, `FUN_00409bfb` | 336× accesses. **inferred** |
| `0x52` | 1 | `net_conn_id` | `NET_SlaveInit`, `FUN_00402640`, `NET_MasterInit` | Accessed in NET_SlaveInit at offset `0x52`. **confirmed** |
| `0x54` | 4 | `unk_54` | `FUN_00406a5e`, `FUN_0040991b`, `HUDFindNearest@8` | 231× accesses. **inferred** |
| `0x58` | 4 | `unk_58` | `FUN_00406a5e`, `FUN_00409910`, `FUN_0040991b` | 156× accesses. **inferred** |
| `0x5A` | 2 | `damage_state` | `FUN_004089a0`, `_DAMAGEDoHit@12`, `_DAMAGEUpdate@0` | Read/written by damage system. **confirmed** |
| `0x5C` | 4 | `unk_5C` | `usnfmain`, `FUN_00406a5e`, `FUN_00409bfb` | 143× accesses. **inferred** |
| `0x60` | 4 | `unk_60` | `FUN_0040991b`, `FUN_00409bfb`, `MenuCreateRemaps` | 166× accesses. **inferred** |
| `0x64` | 4 | `altitude_f24` | `slave_process_pkt`, `HUDDrawAlt`, `FUN_00409bfb` | Read by `?HUDDrawAlt@@YIXXZ`; F24.8 format. **confirmed** |
| `0x65` | 1 | `net_state` | `NET_SlaveInit`, `handle_slave_connection_failed`, `slave_process_pkt` | Network connection state byte. **confirmed** |
| `0x68` | 4 | `unk_68` | `FUN_00406a5e`, `FUN_00409bfb`, `FUN_0040e3a0` | 123× accesses. **inferred** |
| `0x6A` | 2 | `damage_init_data` | `_DAMAGEInit@0`, `FUN_00444ca0`, `FUN_0044aa20` | Read during damage system init. **confirmed** |
| `0x6B` | 1 | `max_speed_ref` | `FUN_0040e470`, `@ArmPlane@4`, `_MaxSpeed@8` | Read by `_MaxSpeed@8`. **confirmed** |
| `0x6C` | 4 | `unk_6C` | `FUN_00406a5e`, `FUN_00409bfb`, `FUN_0040b320` | 99× accesses. **inferred** |
| `0x6E` | 2 | `fm_plane_fields` | `_ScreenDump@0`, `_FMUpdatePlaneFields@0` | Written by `_FMUpdatePlaneFields@0`. **confirmed** |
| `0x70` | 4 | `unk_70` | `slave_process_pkt`, `FUN_00402640`, `FUN_00406a5e` | 121× accesses. **inferred** |
| `0x74` | 4 | `unk_74` | `slave_process_pkt`, `FUN_00402640`, `FUN_00406a5e` | 219× accesses. **inferred** |
| `0x78` | 4 | `hud_message_ptr` | `slave_process_pkt`, `_HUDMessage@4`, `FUN_00406a5e` | Read by `_HUDMessage@4`. **confirmed** |
| `0x7C` | 4 | `unk_7C` | `FlyingLoop`, `FUN_00406a5e`, `FUN_00409bfb` | 88× accesses. **inferred** |
| `0x7D` | 1 | `call_damage_proc` | `@CallDamageProc@4`, `FUN_00463f30`, `GToTurn@8` | Read by `@CallDamageProc@4`. **confirmed** |
| `0x7F` | 1 | `map_side` | `FUN_00406a5e`, `_ServicePlayer@0`, `@MAPSetSide@4` | Written by `@MAPSetSide@4`. **confirmed** |
| `0x80` | 4 | `unk_80` | `FUN_00401580`, `FUN_00402640`, `usnfmain` | 618× accesses. **inferred** |
| `0x84` | 4 | `unk_84` | `FUN_00406a5e`, `_DAMAGEInit@0`, `_ServicePlayer@0` | 75× accesses. **inferred** |
| `0x86` | 2 | `damage_init2` | `_DAMAGEInit@0`, `FUN_0041769c`, `FUN_0043db40` | Written during damage init. **confirmed** |
| `0x88` | 4 | `unk_88` | `FUN_00406a5e`, `_DAMAGEInit@0`, `@ArmPlane@4` | 66× accesses. **inferred** |
| `0x8C` | 4 | `move_obj_ptr` | `FUN_0042d050`, `FUN_00430a90`, `_MoveObj@0` | Read by `_MoveObj@0`; likely next-object link or move data. **confirmed** |
| `0x8D` | 1 | `crc_data` | `FUN_00428340`, `ComputeCRC@@YGJPAUOBJ_TYPE@@` | Read by `?ComputeCRC@@YGJPAUOBJ_TYPE@@@Z`. **confirmed** |
| `0x8E` | 1 | `graphic_flags` | `FUN_0043d69b`, `_GRAPHICAddYourObjs@4` | Read by graphic object add functions. **confirmed** |
| `0x90` | 4 | `unk_90` | `ShellSetup`, `FUN_0042d050`, `_MoveObj@0` | 32× accesses. **inferred** |
| `0x94` | 4 | `unk_94` | `FUN_00418a50`, `@GetNames@4`, `FUN_0042d050` | 49× accesses. **inferred** |
| `0x96` | 2 | `altitude_display` | `HUDDrawAlt`, `FUN_0043faf0`, `FUN_004449f0` | Read by `?HUDDrawAlt@@YIXXZ`. **confirmed** |
| `0x98` | 4 | `unk_98` | `FUN_0041769c`, `FUN_0042d050`, `_MoveObj@0` | 38× accesses. **inferred** |
| `0x9C` | 4 | `unk_9C` | `FUN_0040d810`, `FUN_0042d050`, `FUN_0042e1d0` | 41× accesses. **inferred** |
| `0x9E` | 2 | `eject_data` | `@PilotScreen@4`, `@EJECTAdd@4` | Written by `@EJECTAdd@4`. **confirmed** |
| `0xA0` | 4 | `hud_draw_data` | `_HUDDraw@4`, `FUN_00406a5e`, `_DAMAGEInit@0` | Read by `_HUDDraw@4`. **confirmed** |
| `0xA4` | 4 | `unk_A4` | `FUN_0040d390`, `FUN_00425358`, `FUN_0042d050` | 93× accesses. **inferred** |
| `0xA6` | 2 | `unk_A6` | `FUN_00406a5e`, `FUN_0040903b`, `FUN_00409f30` | 93× accesses. **inferred** |
| `0xA8` | 4 | `unk_A8` | `FUN_00420bb0`, `FUN_0042d050`, `_MoveObj@0` | 90× accesses. **inferred** |
| `0xAA` | 2 | `damage_hit_data` | `_DAMAGEInit@0`, `_DAMAGEDoHit@12`, `FUN_00417530` | Written by `_DAMAGEDoHit@12`. **confirmed** |
| `0xAC` | 4 | `unk_AC` | `_HUDInit@0`, `FUN_0040d810`, `FUN_0040e3a0` | 41× accesses. **inferred** |
| `0xAD` | 1 | `plane_type_change` | `_DAMAGEInit@0`, `_ChangePlaneType@12`, `_FMGetAcc@12` | Read by `_ChangePlaneType@12`. **confirmed** |
| `0xAE` | 2 | `unk_AE` | `FUN_0040e470`, `FUN_00428a3b`, `_SelectRepairPlane@16` | 45× accesses. **inferred** |
| `0xB0` | 4 | `unk_B0` | `FUN_0040e3a0`, `_DAMAGEInit@0`, `FUN_0042d050` | 51× accesses. **inferred** |
| `0xB3` | 1 | `reaction_data` | `@GetNames@4`, `FUN_00426696`, `@Reaction@12` | Read by `@Reaction@12`. **confirmed** |
| `0xB4` | 4 | `unk_B4` | `FUN_00406a5e`, `HUDDrawRangeInfo`, `FUN_0040903b` | 84× accesses. **inferred** |
| `0xB6` | 2 | `stability` | `HUDDrawHeading`, `HUDDrawRangeInfo`, `HUDSetStability` | Written by `?HUDSetStability@@YIJXZ`. **confirmed** |
| `0xB8` | 4 | `unk_B8` | `FUN_0041c4f0`, `FUN_0042d050`, `_MoveObj@0` | 39× accesses. **inferred** |
| `0xBA` | 2 | `flight_key_state` | `FUN_00402640`, `@FlightKey@4`, `FUN_00418a50` | Read by `@FlightKey@4`. **confirmed** |
| `0xBC` | 4 | `unk_BC` | `FUN_00417150`, `@ArmPlane@4`, `FUN_004242a0` | 44× accesses. **inferred** |
| `0xC0` | 4 | `ifm_time` | `usnfmain`, `FlyingLoop`, `IFMSetTime@@YAXD@Z` | Written by `?IFMSetTime@@YAXD@Z`. **confirmed** |
| `0xC8` | 4 | `slave_conn_fail` | `handle_slave_connection_failed`, `FUN_00406a5e` | Read on slave connection failure. **confirmed** |
| `0xCC` | 4 | `msg_chatter_ptr` | `FUN_00409bfb`, `_MSGSendChatter@24`, `FUN_004267e4` | Read by `_MSGSendChatter@24`. **confirmed** |
| `0xD0` | 4 | `unk_D0` | `FUN_00401e30`, `FUN_00415e30`, `_MoveObj@0` | 33× accesses. **inferred** |
| `0xD5` | 1 | `msg_state` | `_MSGInit@0`, `_MSGSend@32`, `_MSGReceive@8` | Read/written by message system. **confirmed** |
| `0xD8` | 4 | `unk_D8` | `handle_slave_connection_failed`, `FUN_004225d4` | 20× accesses. **inferred** |
| `0xDC` | 4 | `unk_DC` | `FUN_00406a5e`, `FUN_0040e470`, `FUN_0042d050` | 34× accesses. **inferred** |
| `0xDE` | 2 | `cpanel_draw` | `FUN_0040d810`, `FUN_0042c9b0`, `_CPDraw@8` | Read by `_CPDraw@8`. **confirmed** |
| `0xE2` | 4 | `cp_skill` | `FUN_004242a0`, `FUN_0043a5c0`, `CPSetSkill@@YAXF@Z` | Written by `?CPSetSkill@@YAXF@Z`. **confirmed** |
| `0xE3` | 4 | `nearest_obj` | `_NearestObj@16`, `FUN_00424de0`, `FUN_0043a5c0` | Read by `_NearestObj@16`. **confirmed** |
| `0xE4` | 4 | `waypoint_ptr` | `FUN_0040d810`, `_MSGSendChatter@24`, `MAPUpdateWPPtrs@8` | Read by `@MAPUpdateWPPtrs@8`. **confirmed** |
| `0xE6` | 2 | `service_player` | `_ServicePlayer@0`, `FUN_00417530`, `_SelectRepairPlane@16` | Read by `_ServicePlayer@0`. **confirmed** |
| `0xE8` | 4 | `msg_chatter2` | `FUN_0040e470`, `_MSGSendChatter@24`, `FUN_004226cb` | Read by `_MSGSendChatter@24`, the same dedicated accessor that confirms `0xCC msg_chatter_ptr`. **confirmed** |
| `0xE9` | 1 | `ap_takeoff_type` | `@FlightKey@4`, `@APTakeoffType@8` | Written by `@APTakeoffType@8`. **confirmed** |
| `0xEA` | 2 | `ap_landing_type` | `InitCobra`, `MainLoop`, `@APLandingType@8` | Written by `@APLandingType@8`. **confirmed** |
| `0xEB` | 1 | `ap_type2` | `@ArmPlane@4`, `WRFogLayerUpdate`, `@APTakeoffType@8` | **confirmed** |
| `0xF0` | 4 | `unk_F0` | `FUN_004242a0`, `FUN_00426696`, `FUN_0042d050` | 62× accesses. **inferred** |
| `0xF1` | 1 | `proj_fire_ref` | `FUN_004aacfe`, `_PROJFire@16`, `_PROJServiceWeapon@24` | Read by `_PROJFire@16`. **confirmed** |
| `0xF4` | 4 | `unk_F4` | `FUN_0040e470`, `_MSGSendChatter@24`, `FUN_004242a0` | 34× accesses. **inferred** |
| `0xF5` | 1 | `bay_state` | `@FMBay@4`, `FUN_004b298e` | Read by `@FMBay@4`; weapon bay open/close. **confirmed** |
| `0xF8` | 4 | `arm_plane_data` | `@ArmPlane@4`, `@GetNames@4`, `FUN_004242a0` | Read by `@ArmPlane@4`. **confirmed** |
| `0xFF` | 1 | `hud_disrupt_flag` | `FUN_00405cd0`, `FUN_00406a5e` | See `HUDSetDisrupt@4` at `0x01` of HUD state. **inferred** |
| `0x100` | 4 | `unk_100` | `_explode`, `FUN_00401180`, `FUN_00401280` | 328× accesses; mirrors low-offset pattern. **inferred** |
| `0x104` | 4 | `menu_bar_data` | `usnfmain`, `MenuDrawBar@@YIXD@Z`, `FUN_0040cb80` | Read by `?MenuDrawBar@@YIXD@Z`. **confirmed** |
| `0x10C` | 4 | `music_on_ptr` | `_MSGSend@32`, `FUN_0042d050`, `MusicOn@@YIXPADF@Z` | Read by `?MusicOn@@YIXPADF@Z`. **confirmed** |
| `0x10D` | 1 | `streamer_ptr` | `FUN_00409bfb`, `_DrawStreamer@12` | Read by `_DrawStreamer@12`. **confirmed** |
| `0x110` | 4 | `unk_110` | `FUN_0040d810`, `_DAMAGEDoHit@12`, `FUN_0041c4f0` | 28× accesses. **inferred** |
| `0x111` | 1 | `ap_park` | `_SelectRepairPlane@16`, `_APClearParks@0`, `_APGetPark@0` | Read by `_APGetPark@0`. **confirmed** |
| `0x113` | 1 | `plane_crash` | `FUN_0043faf0`, `_PLANECrash@4` | Read by `_PLANECrash@4`. **confirmed** |
| `0x115` | 1 | `proj_speed_ref` | `MenuCreateRemaps`, `_PROJSpeed@8` | Read by `_PROJSpeed@8`. **confirmed** |
| `0x118` | 4 | `unk_118` | `MenuCreateRemaps`, `FUN_0040d810`, `FUN_0042d050` | 38× accesses. **inferred** |
| `0x11A` | 2 | `menu_state` | `MenuStartUp@@YGXPADJJDJ@Z`, `FUN_0040cb80`, `_CPDraw@8` | Read by menu startup. **confirmed** |
| `0x11C` | 4 | `unk_11C` | `MenuStartUp`, `FUN_0040c990`, `MenuSteelRect` | 29× accesses. **inferred** |

---

### `PROJ_TYPE` — projectile / missile **type** extension (census)

Offsets into the **type record** for missile and projectile types — *not* into an entity allocation, as an earlier revision of this document claimed: `_PROJSpeed@8` is called as `_PROJSpeed(_cgt, …)`, and `_cgt` is the type-record mirror. Scanned in the range `0x4c0000`–`0x4c3000` (the PROJ subsystem code block); only offsets `0x50`–`0x7F` are listed, because the scan was bounded there — that bound, not a structural boundary, is where the "extension starts at `0x50`" idea came from.

**Scan range:** `0x4c0000`–`0x4c3000`, offsets `0x50`–`0x7F`

| Offset | Size | Field name (inferred) | Accessor functions | Notes |
|--------|------|-----------------------|--------------------|-------|
| `0x50` | 4 | `proj_target_ptr` | `_PROJFire@16`, `_PROJInFOV@40`, `FUN_004c2b5a` | Read at fire and seeker FOV check. **confirmed** |
| `0x54` | 4 | `unk_54` | (1× only) | **inferred** |
| `0x55` | 1 | `proj_hit_flag` | `_PROJHit@8` | Written on impact. **confirmed** |
| `0x57` | 1 | `unk_57` | `FUN_004c0a9d` | **inferred** |
| `0x58` | 4 | `proj_fov_data` | `_PROJInFOV@40`, `FUN_004c2b5a` | Read by seeker FOV test. **confirmed** |
| `0x5A` | 2 | `unk_5A` | `FUN_004c0a9d` | **inferred** |
| `0x5C` | 4 | `proj_guidance` | `FUN_004c24b0`, `_PROJInFOV@40` | Read by guidance update. **inferred** |
| `0x60` | 4 | `proj_guidance2` | `FUN_004c24b0`, `_PROJInFOV@40` | Read by guidance update (5×). **inferred** |
| `0x64` | 4 | `proj_speed` | `FUN_004c0a9d`, `_PROJSpeed@8`, `_PROJHit@8`, `_PROJFire@16` | Read by `_PROJSpeed@8`; 25× accesses. **confirmed** |
| `0x67` | 1 | `proj_speed_byte` | `_PROJSpeed@8` | Byte component of speed. **confirmed** |
| `0x68` | 1 | `proj_fov_byte` | `_PROJInFOV@40` | Seeker cone angle. **confirmed** |
| `0x6B` | 1 | `unk_6B` | `_PROJSpeed@8` | **confirmed** (same accessor) |
| `0x6C` | 1 | `proj_fov2` | `_PROJInFOV@40` | Second FOV parameter. **confirmed** |
| `0x6E` | 1 | `unk_6E` | `FUN_004c2b5a` | **inferred** |
| `0x70` | 4 | `proj_guidance3` | `FUN_004c24b0`, `_PROJInFOV@40` | **inferred** |
| `0x72` | 2 | `unk_72` | (1× only) | **inferred** |
| `0x74` | 4 | `proj_fire_data` | `FUN_004c0a9d`, `_PROJInFOV@40` | **inferred** |
| `0x7F` | 1 | `proj_fire_sound` | `FUN_004c1c10`, `_PROJFireSound@4` | Written when firing sound is triggered. **confirmed** |

---

### `PT_TYPE` — aircraft performance type, `.PT` (census)

Performance type data loaded from `.PT` files on disk (see `architecture.md` BRF type byte `5`). At runtime the struct pointer is obtained from `@T_Load@4` / `@T_GetLeaf@12` and passed to the flight model. Key physics globals (`DAT_0050d3xx` range) in `AnalyzePhysics.txt` map to fields in this struct.

**Disk source:** `.PT` files (BRF struct_type `5`)

**Scan range:** `0x400000`–`0x540000`, offsets `0x00`–`0xC0`

Offsets `0x00`–`0x4F` are shared with the `entity` header (same access patterns); the PT-specific physics fields begin around `0x50`. The table below lists the complete distinct set found in the PT_TYPE scan; offsets that exactly match the `entity` scan are marked as shared.

| Offset | Size | Field name (inferred) | Accessor functions | Notes |
|--------|------|-----------------------|--------------------|-------|
| `0x00`–`0x4F` | — | *(entity header fields)* | — | Shared with `entity`; see §1 above |
| `0x50` | 4 | `pt_perf_base` | `FUN_00406a5e`, `FUN_00409760`, `FUN_0040991b` | First PT-specific dword after header. **inferred** |
| `0x5A` | 2 | `pt_damage` | `FUN_004089a0`, `_DAMAGEDoHit@12`, `_DAMAGEUpdate@0` | Written by damage system. **confirmed** |
| `0x5C` | 4 | `pt_usnf_data` | `usnfmain`, `FUN_00406a5e`, `FUN_0040991b` | **inferred** |
| `0x60` | 4 | `pt_unk_60` | `FUN_0040991b`, `FUN_00409bfb`, `MenuCreateRemaps` | **inferred** |
| `0x64` | 4 | `pt_alt_f24` | `slave_process_pkt`, `HUDDrawAlt`, `FUN_0040991b` | Altitude in F24.8. **confirmed** |
| `0x68` | 4 | `pt_unk_68` | `FUN_00406a5e`, `FUN_00409bfb`, `FUN_0040e3a0` | **inferred** |
| `0x6B` | 1 | `pt_max_speed` | `FUN_0040e470`, `_MaxSpeed@8` | Read by `_MaxSpeed@8`. **confirmed** |
| `0x6E` | 2 | `pt_fm_fields` | `_FMUpdatePlaneFields@0` | Written by flight model update. **confirmed** |
| `0x70` | 4 | `pt_unk_70` | `slave_process_pkt`, `FUN_00402640` | **inferred** |
| `0x74` | 4 | `pt_unk_74` | `slave_process_pkt`, `FUN_00402640` | **inferred** |
| `0x78` | 4 | `pt_hud_msg` | `_HUDMessage@4`, `FUN_00406a5e` | **confirmed** |
| `0x7C` | 4 | `pt_unk_7C` | `FlyingLoop`, `FUN_00406a5e` | **inferred** |
| `0x80` | 4 | `pt_unk_80` | `FUN_00401580`, `FUN_00402640`, `usnfmain` | 618× accesses. **inferred** |
| `0x84` | 4 | `pt_damage_init` | `_DAMAGEInit@0`, `_ServicePlayer@0` | **confirmed** |
| `0x88` | 4 | `pt_arm_plane` | `@ArmPlane@4`, `FUN_004267e4` | Read by `@ArmPlane@4`. **confirmed** |
| `0x8C` | 4 | `pt_move_ptr` | `FUN_0042d050`, `_MoveObj@0` | **confirmed** |
| `0x90` | 4 | `pt_unk_90` | `ShellSetup`, `FUN_0042d050`, `_MoveObj@0` | **inferred** |
| `0x94` | 4 | `pt_names` | `@GetNames@4`, `FUN_0042d050` | Read by `@GetNames@4`; aircraft name string ptr. **confirmed** |
| `0x96` | 2 | `pt_alt_display` | `HUDDrawAlt`, `FUN_0043faf0` | **confirmed** |
| `0xA0` | 4 | `pt_hud_draw` | `_HUDDraw@4`, `FUN_0040d810` | **confirmed** |
| `0xA4` | 4 | `pt_unk_A4` | `FUN_0040d390`, `FUN_0042d050` | **inferred** |
| `0xA8` | 4 | `pt_unk_A8` | `_MoveObj@0`, `FUN_0043faf0` | **inferred** |
| `0xAD` | 1 | `pt_change_type` | `_ChangePlaneType@12`, `_FMGetAcc@12` | Read by `_ChangePlaneType@12`. **confirmed** |
| `0xB0` | 4 | `pt_unk_B0` | `_DAMAGEInit@0`, `FUN_0044a910` | **inferred** |
| `0xB3` | 1 | `pt_reaction` | `@Reaction@12`, `@GetNames@4` | **confirmed** |
| `0xB6` | 2 | `pt_stability` | `HUDDrawHeading`, `HUDSetStability` | **confirmed** |
| `0xBC` | 4 | `pt_unk_BC` | `FUN_00417150`, `@ArmPlane@4` | **inferred** |
| `0xC0` | 4 | `pt_time` | `usnfmain`, `FlyingLoop`, `IFMSetTime@@YAXD@Z` | Written by `?IFMSetTime@@YAXD@Z`. **confirmed** |

---

### `GV_TYPE` — ground-vehicle **type** extension (census)

Offsets into the **type record** for ground-vehicle types (again: type data, not an entity overlay). Scanned exclusively in the MP subsystem code block (`0x470000`–`0x480000`), which handles multiplayer synchronization of ground vehicles. The MP message functions (`MPMsgSend`, `MPGraphicAdd*`, `MPKey`) drive nearly all accesses.

**Scan range:** `0x470000`–`0x480000`, offsets `0x00`–`0x80`

| Offset | Size | Field name (inferred) | Accessor functions | Notes |
|--------|------|-----------------------|--------------------|-------|
| `0x00` | 1 | `gv_type_id` | `FUN_0047001c`, `MPMsgSend`, `MPGraphicAddExp` | Object type identifier. 443× accesses. **inferred** |
| `0x01` | 1 | `gv_flags` | `MPProjAdd`, `FUN_0047001c`, `MPEjectAdd` | Primary flags. 687× accesses. **inferred** |
| `0x04` | 4 | `gv_id` | `MPSetPaused`, `FUN_0047001c`, `MPEjectAdd` | Vehicle index or ID. 610× accesses. **inferred** |
| `0x08` | 4 | `gv_pos` | `FUN_0047001c`, `MPEjectAdd`, `FUN_004701ac` | Position data. 563× accesses. **inferred** |
| `0x0C` | 4 | `gv_state` | `FUN_0047001c`, `MPPrepareForInterp`, `FUN_0047025c` | Interpolation/movement state. 168× accesses. **inferred** |
| `0x10` | 4 | `gv_graphic` | `FUN_0047025c`, `MPGraphicAddCrater`, `MPGraphicAddSpecialDebris` | Graphic data reference. 262× accesses. **inferred** |
| `0x14` | 4 | `gv_unk_14` | `FUN_004701ac`, `FUN_0047025c`, `MPMsgSend` | 203× accesses. **inferred** |
| `0x18` | 4 | `gv_net_data` | `MPEjectAdd`, `FUN_004701ac`, `FUN_0047025c` | Network sync data. 160× accesses. **inferred** |
| `0x1C` | 4 | `gv_unk_1C` | `FUN_0047001c`, `FUN_004701ac`, `MPMsgSend` | 179× accesses. **inferred** |
| `0x20` | 4 | `gv_unk_20` | `FUN_0047025c`, `MPMsgSend`, `MPGraphicAddExp` | 149× accesses. **inferred** |
| `0x21` | 1 | `gv_hulk_flag` | `MPGraphicAddHulk`, `_GVEventProc`, `FUN_0047759b` | Set on vehicle destruction. **confirmed** |
| `0x22` | 1 | `gv_unk_22` | `MPMsgSend`, `MPLaunchDevice`, `MPGraphicAddFire` | **inferred** |
| `0x24` | 4 | `gv_unk_24` | `FUN_00470560`, `MPMsgSend`, `MPGraphicAddSmoke` | 99× accesses. **inferred** |
| `0x28` | 4 | `gv_unk_28` | `FUN_00470560`, `MPMsgSend`, `MPGraphicAddExp` | 95× accesses. **inferred** |
| `0x2C` | 4 | `gv_proj_add` | `MPProjAdd`, `FUN_0047001c`, `MPManAdd` | Read by `MPProjAdd`; projectile launch data. **confirmed** |
| `0x2D` | 1 | `gv_chatter_msg` | `MPMsgSend`, `MPGraphicAddSmokeAdder`, `MPKillStats` | Kill stats message data. **confirmed** |
| `0x30` | 4 | `gv_unk_30` | `FUN_00470560`, `MPGraphicAddExp`, `MPGraphicAddSmoke` | 73× accesses. **inferred** |
| `0x32` | 2 | `gv_event_proc` | `FUN_0047267c`, `_GVEventProc`, `_CheckLandingParms@0` | Read by `_GVEventProc`. **confirmed** |
| `0x34` | 4 | `gv_unk_34` | `FUN_00470560`, `MPGraphicAddSmoke`, `MPGraphicAddDebris` | 60× accesses. **inferred** |
| `0x38` | 4 | `gv_unk_38` | `FUN_00470560`, `MPGraphicAddSmoke`, `MPPlayerChoseSide` | 52× accesses. **inferred** |
| `0x3C` | 4 | `gv_unk_3C` | `FUN_004701ac`, `FUN_00470560`, `MPGraphicAddFire` | 47× accesses. **inferred** |
| `0x3F` | 1 | `gv_fort_btn2` | `MPFortButtonText2`, `FUN_0047759b`, `@COSig@20` | Accessed by the dedicated `MPFortButtonText2` (fort-menu button-text 2), consistent with the other `MP*`-named GV fields. **confirmed** |
| `0x40` | 4 | `gv_unk_40` | `FUN_004701ac`, `MPGraphicAddFire`, `MPSendAntiCheat` | 70× accesses. **inferred** |
| `0x44` | 4 | `gv_unk_44` | `FUN_004701ac`, `FUN_00470560`, `MPGraphicAddFire` | 36× accesses. **inferred** |
| `0x48` | 4 | `gv_unk_48` | `FUN_004701ac`, `FUN_00470560`, `MPGraphicAddSmokeAdder` | 24× accesses. **inferred** |
| `0x4B` | 1 | `gv_fuel` | `MPSetFuel@@YIXG@Z`, `FUN_0047759b`, `_COBrv@0` | Written by `?MPSetFuel@@YIXG@Z`. **confirmed** |
| `0x4C` | 4 | `gv_hardpoints` | `FUN_004701ac`, `FUN_00470560`, `MPSetHardpoints` | Read by `?MPSetHardpoints@@YIXG@Z`. **confirmed** |
| `0x4D` | 1 | `gv_waypoints` | `MPSetWaypoints@@YIXGPAUWAYPOINT@@@Z` | Written by `?MPSetWaypoints@@YIXGPAUWAYPOINT@@@Z`. **confirmed** |
| `0x50` | 4 | `gv_unk_50` | `MPGraphicAddSmokeAdder`, `FUN_00472130`, `_FMFlight@0` | 16× accesses. **inferred** |
| `0x52` | 1 | `gv_cn_print` | `CN_Print@@YAXPAE@Z` | Read by `?CN_Print@@YAXPAE@Z`. **confirmed** |
| `0x54` | 4 | `gv_unk_54` | `MPGraphicAddSmokeAdder`, `FUN_004715bc`, `FUN_0047267c` | **inferred** |
| `0x5C` | 4 | `gv_clear_surface` | `CDirDrawSurface::Clear`, `_LibStartUp` | Reached only by `CDirDrawSurface::Clear` — a method of the DirectDraw surface wrapper class (`0x41Dxxx`), not the GV struct; probable cross-struct contamination (see [Confidence](#confidence-in-the-census-tables), category 3). Not a genuine `GV_TYPE` field. **inferred** |
| `0x60` | 4 | `gv_cn_factory` | `CN_SetFactoryDefaults` | Network config factory defaults. **confirmed** |
| `0x64` | 4 | `gv_flight_menu` | `MPKey@@YIGG@Z`, `_FlightMenu` | Read by `_FlightMenu`. **confirmed** |
| `0x78` | 4 | `gv_ddraw_surface` | `FUN_004735d0`, `CDirDrawSurface::Create`, `CDirDrawSurface::SetEntries` | Accessed by `CDirDrawSurface::Create`/`SetEntries` (DirectDraw wrapper methods), not GV logic; probable cross-struct contamination (category 3). Not a genuine `GV_TYPE` field. **inferred** |
| `0x7C` | 4 | `gv_ddraw_surface2` | `MPHUDMessage`, `CDirDrawSurface::Create`, `CDirDrawSurface::SetEntries` | Same `CDirDrawSurface::*` contamination as `0x78` (category 3). Not a genuine `GV_TYPE` field. **inferred** |
| `0x7F` | 1 | `gv_disconnect` | `MPMsgSend`, `FUN_0047267c`, `MPCheckDisconnect` | Read by `?MPCheckDisconnect@@YGDXZ`. **confirmed** |
| `0x80` | 4 | `gv_mp_key` | `FUN_0047025c`, `FUN_00471bdf`, `MPKey@@YIGG@Z` | Read by `?MPKey@@YIGG@Z`. **confirmed** |

---

### `OT_TYPE` — ordnance type, `.OT` (census — **superseded**, see above)

Type data loaded from `.OT` files (BRF struct_type `1`). The scan for this struct covers the same broad address range as `entity`; offsets below `0x60` are listed since the scan was bounded there. Offsets `0x00`–`0x4F` strongly overlap the `entity` header.

**Disk source:** `.OT` files (BRF struct_type `1`)  
**Scan range:** `0x400000`–`0x540000`, offsets `0x00`–`0x60`

| Offset | Size | Field name (inferred) | Accessor functions | Notes |
|--------|------|-----------------------|--------------------|-------|
| `0x00`–`0x4F` | — | *(entity header fields)* | — | See §1 above |
| `0x50` | 4 | `ot_perf_base` | `FUN_00406a5e`, `FUN_00409760`, `FUN_0040991b` | First OT-specific offset after shared header. **inferred** |
| `0x5A` | 2 | `ot_damage` | `FUN_004089a0`, `_DAMAGEDoHit@12`, `_DAMAGEUpdate@0` | Damage hit data. **confirmed** |
| `0x5C` | 4 | `ot_unk_5C` | `usnfmain`, `FUN_00406a5e`, `FUN_0040991b` | **inferred** |
| `0x5D` | 1 | `ot_flight_key` | `@FlightKey@4`, `@ArmPlane@4`, `FUN_00422a71` | Read by `@FlightKey@4`. **confirmed** |
| `0x5E` | 2 | `ot_unk_5E` | `FUN_00406a5e`, `@StringIsNumber@4`, `@StringToNumber@4` | **inferred** |
| `0x5F` | 1 | `ot_map_draw` | `FUN_0040903b`, `FUN_00409f30`, `MAPDrawGrid` | Read by `?MAPDrawGrid@@YGXXZ`. **confirmed** |
| `0x60` | 4 | `ot_menu_data` | `FUN_0040991b`, `FUN_00409bfb`, `MenuCreateRemaps` | Read during menu remap. **inferred** |

---

### `NT_TYPE` — nav target, `.NT` (census — **superseded**, see above)

Type data loaded from `.NT` files (BRF struct_type `3`). Like `OT_TYPE`, the scan covers `0x00`–`0x60`. The accessor pattern through the shared range is identical to `entity`; NT-specific behavior appears at `0x5D`–`0x60`.

**Disk source:** `.NT` files (BRF struct_type `3`)  
**Scan range:** `0x400000`–`0x540000`, offsets `0x00`–`0x60`

| Offset | Size | Field name (inferred) | Accessor functions | Notes |
|--------|------|-----------------------|--------------------|-------|
| `0x00`–`0x4F` | — | *(entity header fields)* | — | See §1 above |
| `0x50` | 4 | `nt_nav_base` | `FUN_00406a5e`, `FUN_00409760`, `FUN_0040991b` | **inferred** |
| `0x5A` | 2 | `nt_damage` | `FUN_004089a0`, `_DAMAGEDoHit@12`, `_DAMAGEUpdate@0` | **confirmed** |
| `0x5C` | 4 | `nt_unk_5C` | `usnfmain`, `FUN_00406a5e` | **inferred** |
| `0x5D` | 1 | `nt_flight_ref` | `@FlightKey@4`, `@ArmPlane@4`, `FUN_00422a71` | **confirmed** |
| `0x5E` | 2 | `nt_unk_5E` | `FUN_00406a5e`, `@StringIsNumber@4`, `@StringToNumber@4` | **inferred** |
| `0x5F` | 1 | `nt_map_draw` | `FUN_0040903b`, `FUN_00409f30`, `MAPDrawGrid` | Read by `?MAPDrawGrid@@YGXXZ`. **confirmed** |
| `0x60` | 4 | `nt_menu_data` | `FUN_0040991b`, `FUN_00409bfb`, `MenuCreateRemaps` | 166× accesses. **inferred** |

---

## Notes on Methodology

The raw data was produced by `RecoverStructs.java`, a Ghidra headless script that enumerates all instructions in the given VA range and records every memory dereference of the form `[reg + constant]` where `reg` holds a known struct pointer argument. The access count column reflects how often each offset was touched across the entire executable scan — higher counts indicate more deeply integrated fields.

**Known limitations — and what they cost:**

- The scan **cannot tell two struct types apart** when they share a pointer-argument convention, and this is not a small caveat: it is what produced the two errors §4 retracts. Because generic engine functions accept any object pointer, a whole-executable scan attributes their accesses to whichever struct it was told it was scanning — which is why the `OT_TYPE` and `NT_TYPE` tables came out *identical*, and why `entity`'s census disagrees with the code about what lives at `+0x04`.
- Restricting the scan to a subsystem's code block (as `PROJ_TYPE` and `GV_TYPE` did) removes the generic-access noise, but it does **not** establish *which family* the pointer belongs to — both were filed as entity overlays when they are in fact type-record extensions.
- Fields accessed only once or twice are often incidental (e.g., CRT helpers scanning memory). Treat single-access entries as tentative.
- Size column values are inferred from the next observed offset; actual padding may differ.

### The mirror method (how §1 and §2 were recovered)

The recovered layouts do not come from this census. They come from the **current-object mirrors**:
`GetCurObj` copies the live record into the fixed buffer `_cg` (and its type record into `_cgt`), so
the code that services an object reaches its fields at **absolute addresses** — `_cg+N` rather than
`obj->field`.

That turns a field access into a directly observable fact:

- the instruction's **operand size proves the field's width** (`MOV AL,[_cg+N]` = 1 byte) — the same
  rule the typed-globals pass rests on;
- the **referencing function proves the subsystem**, which is what separates the common region
  (touched by many subsystems) from the class extension (touched by one class's code);
- and the **family is never in doubt**, because `_cg` and `_cgt` are different addresses.

The one thing it cannot do is prove a *negative*: code that reaches a field through a register
pointer leaves no absolute reference, so **absence from the census proves nothing**. Every byte the
mirror does not account for therefore stays `reserved` in `fa_types.h` rather than being invented.
