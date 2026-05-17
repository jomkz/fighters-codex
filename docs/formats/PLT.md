# Pilot Save -- Pilot Profile (.P)

`PLTnnn.P` files (e.g. `PLT441.P`, `PLT628.P`) are **binary pilot save files** written
by the FA engine. They store the persistent state for each pilot slot.

Unlike all other FA data, pilot files are stored **directly in the FA install directory**,
not inside any `.LIB` archive. The filename number does not imply a slot sequence — it
appears to be a randomly assigned identifier.

---

## File Size

All observed files are exactly **9,696 bytes**.

## Field Layout

Offsets and lengths confirmed by cross-comparing three pilot files with known values.

### Identity block (offset 0x00 – 0xAF)

| Offset | Size | Type | Field |
|--------|------|------|-------|
| `0x00` | 1 | u8 | Type / version tag — observed: `0x0F` |
| `0x01` | 63 | char[] | Pilot name, null-padded |
| `0x40` | 32 | char[] | Callsign, null-padded |
| `0x61` | 13 | char[] | Callsign voice file (e.g. `^ACID.5K`), null-padded |
| `0x6E` | 13 | char[] | Nose art ID (e.g. `NOSE01`), null-padded |
| `0x7B` | 13 | char[] | Left wing decal ID (e.g. `LEFT03`), null-padded |
| `0x88` | 13 | char[] | Right wing decal ID (e.g. `RIGHT03`), null-padded |
| `0x95` | 13 | char[] | Pilot portrait ID (e.g. `PILOT02`), null-padded |
| `0xA2` | 14 | char[] | Rank string (e.g. `2nd Lieutenant`), null-padded |

### Campaign block (observed starting ~`0x0D7F` for active pilots)

The layout between `0xB0` and the campaign block is not yet mapped. The campaign block
contains null-terminated strings for:

- Campaign `.CAM` filename (e.g. `EGYPT.CAM`)
- Campaign display name (e.g. `Egypt 1998`)
- Assigned aircraft `.PT` reference (e.g. `F22.PT`)
- Available aircraft pool (`.PT` references)
- Ordnance inventory: `.JT` filename + u8 quantity per item (up to 50 entries)
- Sensor/ECM loadout (`.SEE`, `.ECM` references)

### Unknown / unmapped regions

- `0xB0` – `0x0D7E`: status flags (Active / Available / MIA / KIA), missions
  completed, missions failed, kill tallies, hit percentages, last-modified timestamp,
  and campaign statistics. Field order and sizes within this range are not yet confirmed.

> **TODO:** Do a second differential pass — see methodology below.

## Differential Mapping — Stats Block (`0xB0`–`0x0D7E`)

Method to systematically map the unmapped 3,279-byte region:

### Pass 1 — Status flags

1. Create a fresh pilot (zero missions). Save. Note the bytes at `0xB0`–`0xBF`.
2. Fly one mission and win. Save. Diff against Pass 1. Bytes that changed from `0x00` → non-zero in this narrow range are status/counter fields.
3. Fly one mission and die (KIA). Diff again. A KIA flag byte should flip.

### Pass 2 — Kill tallies

1. Start from a known state (zero kills). Record bytes `0xB0`–`0x0D7E`.
2. Get exactly 1 air kill, 1 ground kill, 1 naval kill. Save after each.
3. Bytes that increment by 1 with each kill type identify the kill counter offsets. Expect u16 or u32 counters.

### Pass 3 — Missions statistics

1. Fly a mission with a known number of shots fired and hits scored.
2. Diff: look for pairs of values where one tracks shots and one tracks hits (hit% is computed, not stored directly).

### Pass 4 — Timestamps

1. Check for a 4-byte value near the end of the unmapped block that increases monotonically and is consistent with a DOS/Win32 timestamp (`time_t` = seconds since 1970-01-01, or a FA-internal tick counter).

### Recommended tools

- **HxD** — load two saves side by side, use Compare → Differ.
- **010 Editor** — create a template as fields are confirmed; track which bytes are decoded.

## Related

- [BRF.md](BRF.md) — `.PT` (plane type) and `.JT` (ordnance) records referenced by name
- [MISSION.md](MISSION.md) — `.MM` map/campaign files referenced by name

---

## Applications

The identity block (offsets `0x01`–`0xAF`) is fully mapped and patchable with a hex
editor. The stats block (`0xB0`–`0x0D7E`) is pending a second differential pass.

- **FATK** — free (abandonware, 1998); original GUI tool with full pilot editing support; requires a compatibility layer on 64-bit Windows
- **HxD** — free, Windows; use with the field table above for manual patching
- **VS Code** + [Hex Editor](https://marketplace.visualstudio.com/items?itemName=ms-vscode.hexeditor) — free, cross-platform; convenient if already using VS Code for text editing
- **010 Editor** `$` — paid; binary templates will allow a fully labelled struct view once the stats block is mapped
