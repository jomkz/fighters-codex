# Installer UI Region Map (.RGN)

`.RGN` files define the clickable regions for the EA installer's bitmap UI screens. They map screen-space rectangles to button actions, allowing the installer's graphical buttons (rendered as PIC/BMP images) to respond to mouse clicks.

## Location

Found in the FA installer directory alongside the installer executable. Not packed into any LIB archive.

## Known Files

| File | Size | Role |
|------|------|------|
| `BUTTONS.RGN` | 3,844 bytes | Main installer button layout |
| `POSTER.RGN` | 324 bytes | Poster/splash screen click regions |

## Format Hypothesis

Binary. Size arithmetic on `POSTER.RGN` strongly constrains the record layout:

```
324 bytes = 4 (header: u32 count) + 32 × 10 (records)
```

Hypothesised record layout (10 bytes each):

```
Offset  Size  Field
------  ----  -----
+0         2  x       u16 LE — left edge (pixels, 640×480 space)
+2         2  y       u16 LE — top edge
+4         2  width   u16 LE
+6         2  height  u16 LE
+8         2  action  u16 LE — button / action index
```

If count = 32 at offset 0, total = 4 + 320 = 324. ✓

`BUTTONS.RGN` at 3,844 bytes: if the same 10-byte record applies, `(3844 − 4) / 10 = 384` records — plausible for a full installer with many screen states. Alternatively the record may carry an extra field in BUTTONS.RGN.

## Verification

1. Open `POSTER.RGN` in a hex editor. Read `u32 LE` at offset 0. If it equals 32, the hypothesis is confirmed.
2. Read record 0: bytes 4–13. Parse as five `u16 LE` values. Verify that (x, y, w, h) fall within 0–640 and 0–480.
3. Cross-reference the bounding box against the installer splash bitmap to confirm the region corresponds to a visible button.

## TODO

- Confirm count = 32 at offset 0 of `POSTER.RGN`
- Parse and document all 32 region records with their action IDs
- Decode `BUTTONS.RGN` using the same record layout; explain the size difference

## Related

- [SSF.md](SSF.md) — EA installer script that references these regions
