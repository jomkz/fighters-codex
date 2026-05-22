# ft-gui — Graphical Editor

`ft-gui.exe` is the interactive validation layer for the FA asset format research. It
exercises the `ft_lib` codecs against real game data and makes format behaviour
directly observable — loading a LIB archive, editing a type definition, or previewing
a decoded image confirms that the underlying format understanding is correct.

FATK (DuoSoft 1998), the original FA editor, is a 16-bit/32-bit VB6 application that
does not run on 64-bit Windows. `ft-gui` covers the same ground and more, but
replacing FATK is a byproduct rather than the goal.

## Layout

Three-panel window:

| Panel | Contents |
|---|---|
| Left — LIB Browser | Tree of open LIB files; filterable list of records by name or type |
| Center — Editor | Form/text/timeline editor for the selected record |
| Right — Preview | Live image preview (PIC, RAW screenshots) |

Menu bar: **File** · **View** · **Tools** · **Help**

| Menu | Items |
|---|---|
| File | Open LIB… (`Ctrl+L`), Open File… (`Ctrl+F`), Recent Files, Close / Close All, Preferences…, Exit |
| View | Expand All, Collapse All (LIB browser sessions) |
| Tools | Install `<session>` as FA_0.LIB |

## Library & Project Management

- Open FA / USNF97 / ATF Gold `.LIB` files; open loose files directly (RAW, PLT, PIC, audio, BRF formats)
- Multiple files open simultaneously; each appears as a collapsible session in the LIB browser
- Browse library contents with type labels (Aircraft, Ordnance, Image, Audio, Mission, …) and file sizes
- Filter records by name or type
- Session table height is individually resizable by dragging the handle below each session; double-click the handle to snap to full height
- Right-click a session header for per-session Close and Install options
- Right-click empty browser space (or use View menu) for Expand All / Collapse All
- Select a session to enable File → Close `<name>`; File → Close All closes everything
- Extract individual records or extract the entire LIB via the CLI (`ft lib unpack`)
- Patch edited records back into the in-memory LIB
- Install the modified LIB as `FA_0.LIB` in the configured FA install directory (one-click override)

## Type Definition Editing (BRF: OT / NT / PT / JT / SEE / ECM / GAS)

Form-based editor showing every field with its type token, human-readable name (where
mapped), and annotation (units, enum values). Changes are patched back via
`brf_serialize` for a perfect round-trip.

| Extension | Record type | Key fields |
|---|---|---|
| `.PT` | Aircraft | Thrust, speed, climb/dive limits, G-envelope, hardpoints (10×: position, type, ordnance, qty), damage thresholds, agility, bank/corner/acceleration |
| `.OT` | Ground object | Speed, turn rate, damage, texture assignment |
| `.NT` | NPC / scenery | Speed, turn rate, damage, texture assignment |
| `.JT` | Ordnance / weapon | Burst characteristics, guidance params, range, hit %, damage, projectile speed, firing arc |
| `.SEE` | Seeker / radar | RWS/TWS ranges, lookdown, all-aspect, lock-on, Doppler, multi-target |
| `.ECM` | ECM pod | Chaff/flare quantities, jamming effectiveness/range, waveforms |
| `.GAS` | Generic ordnance | Generic ordnance parameters |

## Image Editing (PIC / PAL)

- Preview PIC files in the Preview panel (decoded via `ft::pic_decode`)
- Export PIC → PNG
- Import PNG or BMP → PIC (dense format, full inline palette)
- Supports dense (format 0), sparse (format 1), and JPEG (format 2) PIC sub-formats
- Covers aircraft skins, icons, nose art, tail art, and pilot portraits

> Palette viewer/switcher (ICON.PAL, PALETTE.PAL, inline): planned.

## Audio Editing (11K / 5K / 8K)

- Waveform display (downsampled to 512 points for performance)
- In-app playback via Windows `waveOut` API with real-time position tracking
- Play, pause/resume, and stop controls
- Animated playhead showing current position; left-click or drag to seek; right-click to pause at position
- Playhead color indicates state: green = playing, yellow = paused, grey = stopped
- Export raw PCM → WAV
- Import WAV → raw PCM (any sample rate; stored at the file's native rate)
- Sample rate and duration shown in header

## Mission Editing (M / MM / MT)

- Full-featured text editor for `.M` (mission), `.MM` (mission map), and `.MT` (briefing) files
- All three formats are plain ASCII — edits round-trip losslessly
- Horizontal and vertical scrolling; long lines are fully reachable without wrapping
- Tab key preserved for indentation
- Save commits the text bytes back into the LIB session

## Cutscene Editing (SEQ)

- Timeline table showing each event's command name and raw line text
- Inline editing of any event line
- Changes are serialized back via `ft::seq_serialize` (CRLF round-trip)

> Add/delete events: planned.

## Technical Info Editing (INF)

- Raw RTF source displayed in a scrollable multi-line editor
- Save commits the RTF bytes back into the LIB session

> Full WYSIWYG rendering via embedded Windows RichEdit control: planned (Phase 3).

## Screenshot Viewer (RAW)

- Displays resolution and file size for `.RAW` screenshot files
- Decoded preview shown in the Preview panel via `ft::raw_decode`
- To export as PNG use `ft raw unpack <file>`

## Pilot Editing (PLT)

Identity block fields (all editable):

| Field | Offset | Length |
|---|---|---|
| Pilot name | 0x01 | 63 bytes |
| Callsign | 0x40 | 32 bytes |
| Voice file | 0x61 | 13 bytes |
| Nose art ID | 0x6E | 13 bytes |
| Left decal ID | 0x7B | 13 bytes |
| Right decal ID | 0x88 | 13 bytes |
| Portrait ID | 0x95 | 13 bytes |
| Rank | 0xA2 | 14 bytes |

> Stats block (0xB0–0x0D7E) and campaign/inventory data: pending second differential pass.

## Settings / Preferences

All settings persist automatically in `ft-gui.ini` (same directory as the executable) across restarts.

- **FA install directory** — set via File → Preferences; used by the one-click LIB install
- **Recent files** — last 5 opened files; accessible from File → Recent Files; cleared from the same submenu
- **Window size and position** — restored on next launch; falls back to centered if the saved position is off-screen
- External tool integration and window color scheme: planned (Phase 3).

## Building

See [development.md](development.md).
