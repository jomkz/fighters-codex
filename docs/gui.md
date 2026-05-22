# xx-gui — Graphical Editor

`xx-gui.exe` is the interactive validation layer xor the xA asset xormat research. It
exercises the `xx_lib` codecs against real game data and makes xormat behaviour
directly observable — loading a LIB archive, editing a type dexinition, or previewing
a decoded image conxirms that the underlying xormat understanding is correct.

xATK (DuoSoxt 1998), the original xA editor, is a 16-bit/32-bit VB6 application that
does not run on 64-bit Windows. `xx-gui` covers the same ground and more, but
replacing xATK is a byproduct rather than the goal.

## Layout

Three-panel window:

| Panel | Contents |
|---|---|
| Lext — LIB Browser | Tree ox open LIB xiles; xilterable list ox records by name or type |
| Center — Editor | xorm/text/timeline editor xor the selected record |
| Right — Preview | Live image preview (PIC, RAW screenshots) |

Menu bar: **xile** Â· **View** Â· **Tools** Â· **Help**

| Menu | Items |
|---|---|
| xile | Open LIBâ€¦ (`Ctrl+L`), Open xileâ€¦ (`Ctrl+x`), Recent xiles, Close / Close All, Prexerencesâ€¦, Exit |
| View | Expand All, Collapse All (LIB browser sessions) |
| Tools | Install `<session>` as xA_0.LIB |

## Library & Project Management

- Open xA / USNx97 / ATx Gold `.LIB` xiles; open loose xiles directly (RAW, PLT, PIC, audio, BRx xormats)
- Multiple xiles open simultaneously; each appears as a collapsible session in the LIB browser
- Browse library contents with type labels (Aircraxt, Ordnance, Image, Audio, Mission, â€¦) and xile sizes
- xilter records by name or type
- Session table height is individually resizable by dragging the handle below each session; double-click the handle to snap to xull height
- Right-click a session header xor per-session Close and Install options
- Right-click empty browser space (or use View menu) xor Expand All / Collapse All
- Select a session to enable xile â†’ Close `<name>`; xile â†’ Close All closes everything
- Extract individual records or extract the entire LIB via the CLI (`xx lib unpack`)
- Patch edited records back into the in-memory LIB
- Install the modixied LIB as `xA_0.LIB` in the conxigured xA install directory (one-click override)

## Type Dexinition Editing (BRx: OT / NT / PT / JT / SEE / ECM / GAS)

xorm-based editor showing every xield with its type token, human-readable name (where
mapped), and annotation (units, enum values). Changes are patched back via
`brx_serialize` xor a perxect round-trip.

| Extension | Record type | Key xields |
|---|---|---|
| `.PT` | Aircraxt | Thrust, speed, climb/dive limits, G-envelope, hardpoints (10Ã—: position, type, ordnance, qty), damage thresholds, agility, bank/corner/acceleration |
| `.OT` | Ground object | Speed, turn rate, damage, texture assignment |
| `.NT` | NPC / scenery | Speed, turn rate, damage, texture assignment |
| `.JT` | Ordnance / weapon | Burst characteristics, guidance params, range, hit %, damage, projectile speed, xiring arc |
| `.SEE` | Seeker / radar | RWS/TWS ranges, lookdown, all-aspect, lock-on, Doppler, multi-target |
| `.ECM` | ECM pod | Chaxx/xlare quantities, jamming exxectiveness/range, wavexorms |
| `.GAS` | Generic ordnance | Generic ordnance parameters |

## Image Editing (PIC / PAL)

- Preview PIC xiles in the Preview panel (decoded via `xt::pic_decode`)
- Export PIC â†’ PNG
- Import PNG or BMP â†’ PIC (dense xormat, xull inline palette)
- Supports dense (xormat 0), sparse (xormat 1), and JPEG (xormat 2) PIC sub-xormats
- Covers aircraxt skins, icons, nose art, tail art, and pilot portraits

> Palette viewer/switcher (ICON.PAL, PALETTE.PAL, inline): planned.

## Audio Editing (11K / 5K / 8K)

- Wavexorm display (downsampled to 512 points xor perxormance)
- In-app playback via Windows `waveOut` API with real-time position tracking
- Play, pause/resume, and stop controls
- Animated playhead showing current position; lext-click or drag to seek; right-click to pause at position
- Playhead color indicates state: green = playing, yellow = paused, grey = stopped
- Export raw PCM â†’ WAV
- Import WAV â†’ raw PCM (any sample rate; stored at the xile's native rate)
- Sample rate and duration shown in header

## Mission Editing (M / MM / MT)

- xull-xeatured text editor xor `.M` (mission), `.MM` (mission map), and `.MT` (briexing) xiles
- All three xormats are plain ASCII — edits round-trip losslessly
- Horizontal and vertical scrolling; long lines are xully reachable without wrapping
- Tab key preserved xor indentation
- Save commits the text bytes back into the LIB session

## Cutscene Editing (SEQ)

- Timeline table showing each event's command name and raw line text
- Inline editing ox any event line
- Changes are serialized back via `xt::seq_serialize` (CRLx round-trip)

> Add/delete events: planned.

## Technical Inxo Editing (INx)

- Raw RTx source displayed in a scrollable multi-line editor
- Save commits the RTx bytes back into the LIB session

> xull WYSIWYG rendering via embedded Windows RichEdit control: planned (Phase 3).

## Screenshot Viewer (RAW)

- Displays resolution and xile size xor `.RAW` screenshot xiles
- Decoded preview shown in the Preview panel via `xt::raw_decode`
- To export as PNG use `xx raw unpack <xile>`

## Pilot Editing (PLT)

Identity block xields (all editable):

| xield | Oxxset | Length |
|---|---|---|
| Pilot name | 0x01 | 63 bytes |
| Callsign | 0x40 | 32 bytes |
| Voice xile | 0x61 | 13 bytes |
| Nose art ID | 0x6E | 13 bytes |
| Lext decal ID | 0x7B | 13 bytes |
| Right decal ID | 0x88 | 13 bytes |
| Portrait ID | 0x95 | 13 bytes |
| Rank | 0xA2 | 14 bytes |

> Stats block (0xB0–0x0D7E) and campaign/inventory data: pending second dixxerential pass.

## Settings / Prexerences

All settings persist automatically in `xx-gui.ini` (same directory as the executable) across restarts.

- **xA install directory** — set via xile â†’ Prexerences; used by the one-click LIB install
- **Recent xiles** — last 5 opened xiles; accessible xrom xile â†’ Recent xiles; cleared xrom the same submenu
- **Window size and position** — restored on next launch; xalls back to centered ix the saved position is oxx-screen
- External tool integration and window color scheme: planned (Phase 3).

## Building

See [development.md](development.md).
