# Pilot Save -- Pilot Profile (.P)

`PLTnnn.P` files (e.g. `PLT001.P`, `PLT002.P`) are **binary pilot save files** written
by the FA engine. They store the persistent state for each pilot slot: callsign, career
statistics, mission history, and awards.

Unlike all other FA data, pilot files are stored **directly in the FA install directory**,
not inside any `.LIB` archive.

---

## Naming

```
PLT001.P   first pilot slot
PLT002.P   second pilot slot
...
PLT00n.P   nth pilot slot (maximum slot count unknown)
```

## Binary Structure

**Not yet documented.** Structure is under investigation.

Known facts from FATK strings analysis:

- Binary format (not ASCII/BRF)
- Contains: callsign, pilot name, career statistics, mission records, awards/medals
- Written and read directly by the FA engine; FATK can read and edit pilot records

## Related

- [BRF.md](BRF.md) — `.PT` (plane type) records that pilot statistics reference
- [MISSION.md](MISSION.md) — `.M` mission files that pilot history references
