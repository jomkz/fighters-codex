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

> **TODO:** Do a second differential pass — compare a fresh pilot file against a played
> pilot file byte-by-byte to map the stats block (kills, missions, hit %, timestamps,
> status flags) within `0xB0`–`0x0D7E`.

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
