# Installer UI Region Map (.RGN)

`.RGN` files define the clickable regions for the EA installer's bitmap UI screens. They map screen-space rectangles to button actions, allowing the installer's graphical buttons (rendered as PIC/BMP images) to respond to mouse clicks.

## Location

Found in the FA installer directory alongside the installer executable. Not packed into any LIB archive.

## Known Files

| File | Size | Role |
|------|------|------|
| `BUTTONS.RGN` | 3,844 bytes | Main installer button layout |
| `POSTER.RGN` | 324 bytes | Poster/splash screen click regions |

## Format

Binary. Structure is not yet decoded. Likely a header with region count followed by an array of records, each containing:
- Screen-space bounding box (x, y, width, height) as 16-bit integers
- Action identifier or button index

## TODO — Deep Dive

- Hex-analyze `POSTER.RGN` (324 bytes = small enough to decode by hand) to identify the record structure
- Verify whether coordinates are absolute pixel offsets or relative to a base resolution (likely 640×480)
- Cross-reference button positions against the corresponding installer bitmap to confirm mapping

## Related

- [SSF.md](SSF.md) — EA installer script that references these regions
