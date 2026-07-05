---
format: HGR
name: Hangar Screen
extensions: [".HGR"]
category: 3d
endianness: little
spec:
  status: partial
  gaps:
    - kind: re-static
      issue: 54
      note: "sub-resource layout beyond the slot tables unmapped"
codec:
  direction: read
  rationale: "engine-code container (hangar DLL, PL family) whose slot-table sub-resource base is not yet pinned (#54): fx_lib surfaces the container geometry and the PIC asset references; slot decoding lands when the sub-resource offset is confirmed"
  lib: [lib/src/hgr.cpp]
  commands: [hgr]
  tests: [tests/test_hgr.cpp]
  fuzz: []
  fixtures:
    synthetic: true
    real_manifest: true
related: [PIC, MNU]
---

# HGR — Hangar Screen (.HGR)

FA_2.LIB contains 2 `.HGR` files. Each defines a hangar screen — the aircraft
selection and loadout interface shown at an airbase. Each is a **Win32 PE DLL**
loaded at runtime; `H_AIRB.HGR` decompresses to 4608 bytes.

## Tools

### fx

```
fx hgr info    <file.HGR>            # container check + referenced PIC assets
fx hgr strings <file.HGR> [-n MIN]   # embedded strings
```

Same MZ + Phar Lap `PL` container family as [CAM](CAM.md). Both shipped
files surface their documented references (`h_airb.PIC` twice +
`SELICONS.PIC`; the carrier variant `h_airb2.PIC` twice + `SELICON3.PIC`).

## File Layout

All multi-byte integers are little-endian.

Win32 PE DLL with an embedded sub-resource. String analysis of `H_AIRB.HGR`
reveals asset references:

- **`h_airb.PIC`** — hangar background image (appears twice, likely for
  foreground and background layers)
- **`SELICONS.PIC`** — aircraft selection icons displayed in the hangar UI

### Slot Entry Layout — confirmed

Aircraft slot entries start at **offset +0x27** from the sub-resource base:
30 × 8-byte entries, read by `FUN_004558f0` (VA 0x4558f0):

| Offset | Type | Field | Notes |
|--------|------|-------|-------|
| `+0x00` | s16 | `x_offset` | Aircraft hangar X position; `-1` = slot unused (empty sentinel) |
| `+0x02` | s16 | `y_offset` | Aircraft hangar Y position |
| `+0x04` | s16 | `angle_index` | Index into angle-correction table `asStack_b` (0-based) |
| `+0x06` | s16 | `occupied` | Slot occupancy flag: `0` = free, `1` = assigned |

Screen position is computed as:
```c
screen_x = asStack_b[angle_index * 2]     + x_offset;
screen_y = asStack_b[angle_index * 2 + 1] + y_offset;
```
where `asStack_b[5]` is a 5-short correction table loaded from the sub-resource.

A separate slot-index table for the carrier path sits at `+0x117` (4 ints per
entry, up to 4 entries).

## File Inventory

| File | Purpose |
|------|---------|
| H_AIRB.HGR | Air base hangar screen |
| H_AIRB2.HGR | Carrier hangar screen (confirmed via FA_2.LIB string scan) |

## Engine Notes

`_SelectPlane` (HGR load trigger, called from `_MISSIONInit2@0`):
1. Calls `FUN_004809d0` — initialises `?hangarName@@3PADA` (`0x004fb1e8`) from
   `DAT_004fbbf0`, and copies `"ord_air3.PIC"` to `armPicName`
2. `pilotName` selects carrier (non-zero) vs. land-base hangar
3. Calls `SelectRepairPlane(0, &hangarName, '\0', '\x01')` — actual HGR file loader

`SelectRepairPlane` (HGR file loader) — confirmed structure:
- Loads HGR DLL via `RMAccess(name, 0x8000)`
- When `param_4 == '\0'` (standard load): skips first 13 bytes (`pcVar5 = pcVar6 + 0xd`)
- Loads embedded sub-resource via `RMAccessHandle(pcVar5, 0x8104)`
- Iterates up to `numItems` aircraft types (capped at 100)
- Builds X coordinate array (`local_640`: 400 shorts) and Y coordinate array
  (`local_320`: 400 shorts) for aircraft screen positions

Slot search (param_1 == '\0', standard assign): finds first slot where
`x_offset != -1` and `occupied == 0`, copies its 8 bytes to output buffer, sets
`occupied = 1`.

Carrier assign path (param_1 != '\0'): walks the slot-index table at
`param_5 + 0x117`; finds first entry pointing to a free slot; marks all
referenced slots as occupied.

## Open Questions

### 1. Sub-resource layout beyond the slot tables

The 13-byte skip, the 30 × 8-byte slot table at +0x27, the 5-short correction
table, and the carrier slot-index table at +0x117 are confirmed; the remainder
of the ~4.5 KB sub-resource (and any other embedded resources in the DLL) is
unmapped.

*Status: open — re-static (#54)*

## Related

**Formats:** [PIC](PIC.md) — `h_airb.PIC` and `SELICONS.PIC` are PIC atlas
files; [MNU](MNU.md) — menus that transition to the hangar screen.
