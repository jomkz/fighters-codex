# Backlog

## 1. PLT Stats Block — remaining gaps (requires gameplay)

Ghidra static analysis (`AnalyzePLT.java`) and diff of 3 fresh pilot saves both returned
all-zeros for all four gaps — no code in FA.EXE accesses them by named address, and the
saves were created before any missions were flown. The codec, GUI, and CLI are done for
the confirmed region (`0x1F80–0x21F7`). The gaps can only be decoded with post-gameplay saves.

**Binary probe test (2026-05-21):** Created 4 `PROBE_GAP*` pilot files with distinctive fill
patterns in each gap region and cycled through them on the pilot records/dossier screen.
**Result: no visible difference.** Confirmed that `FUN_004674f0` (pilot card renderer) does
not read from any of the four gap regions — it only accesses `+0xC2`, `+0x5AF`, `+0xD8C`,
`+0xDAC`, `+0x1F88`. The pilot records screen cannot be used as a probe target for gaps 1, 3, or 4.

Requires: launch FA.EXE, fly missions, save, diff against the fresh saves. Suggested screens
to observe after gameplay: mission debrief overlay, campaign map status panel, end-of-campaign
summary.

- `0xB0–0xC1` (18 bytes) — advance pilot rank or score; diff. Adjacent to rank string —
  likely rank index, cumulative score tier, or medal count set when rank is awarded.
- `0xCF–0x5AE` (1,344 bytes) — fly 3–5 missions; diff mission log growth. Structure is
  understood (null-terminated strings read by `FUN_004674f0` from `+0x5AF`); needs content.
- `0x2018–0x20B7` (160 bytes) — fly missions with kills; diff kill/score totals. Sits
  between kill tallies and weapon accuracy — likely additional kill subcategories or a
  per-mission score history array.
- `0x21F8–0x25DF` (~1,000 bytes) — complete a fort-assault mission; diff. Fort-mission
  scratch globals confirmed by RE but flush path into PILOT struct not found statically.
