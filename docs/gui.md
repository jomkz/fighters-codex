# fx-gui — Graphical Editor

`fx-gui` is the interactive validation layer for the FA asset format research. It
exercises the `fx_lib` codecs against real game data and makes format behaviour
directly observable — loading a LIB archive, editing a type definition, or previewing
a decoded image confirms that the underlying format understanding is correct.

FATK (DuoSoft 1998), the original FA editor, is a 16-bit/32-bit VB6 application that
does not run on 64-bit Windows. `fx-gui` covers the same ground and more, but
replacing FATK is a byproduct rather than the goal.

## Platforms

`fx-gui` runs natively on Linux and Windows from the same code: SDL3 windowing with
an OpenGL 3.3 core renderer through Dear ImGui, and miniaudio for audio preview
(backend rationale in [ADR-0001](adr/0001-fx-gui-sdl3-opengl3-miniaudio.md)).

- **Theming** — Auto follows the desktop's dark/light preference (via
  `SDL_GetSystemTheme`) and switches live; Dark/Light can be forced in
  Preferences. On bare X11 without a desktop portal the system preference is
  unknown and Auto falls back to dark.
- **DPI** — UI metrics and fonts scale to the window's display scale, and
  rescale live when the window moves to a display with a different scale.
- **Wayland** — window *position* cannot be saved or restored (a Wayland
  design decision); size and maximized state still persist.
- **`--smoke [LIB ...]`** — headless self-check: with no arguments, renders
  three frames without showing a window (falling back to SDL's offscreen
  driver when no display server exists) and exits 0; CI runs it as the
  `gui_smoke` ctest. Given LIB paths, it opens each archive and cycles every
  entry through its editor and the preview — one rendered frame per record —
  exercising extraction, every parser, and the GPU upload paths against real
  game data.

On Windows, `fx-gui.exe` is a `WIN32`-subsystem (GUI) binary, so shells launch
it detached: a bare `--smoke` invocation from PowerShell prints nothing,
returns immediately, and never sets `$LASTEXITCODE`, while the sweep runs
invisibly in the background. Pipe the output so the shell attaches stdout and
waits for the exit code:

```powershell
$fa = "C:\path\to\Fighters Anthology"
build\gui\Release\fx-gui.exe --smoke (Get-ChildItem "$fa\*.lib").FullName | Out-Host
echo $LASTEXITCODE   # expect 0, with one "swept" line per LIB
```

FA installs mix filename case (`FA_1.LIB`, `fa_7.lib`). `Get-ChildItem`
matches case-insensitively; a case-sensitive POSIX glob like `*.LIB` silently
misses the lowercase ones.

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
- Extract individual records or extract the entire LIB via the CLI (`fx lib unpack`)
- Patch edited records back into the in-memory LIB
- Install the modified LIB as `FA_0.LIB` in the configured FA install directory (one-click override)

## Type Definition Editing (BRF: OT / NT / PT / JT / SEE / ECM / GAS)

Form-based editor showing every field with its type token, human-readable name (where
mapped), and annotation (units, enum values). Changes are patched back via
`brf_serialize` for a perfect round-trip.

| Extension | Record type | Key fields |
|---|---|---|
| `.PT` | Aircraft | Thrust, speed, climb/dive limits, G-envelope, hardpoints (up to 9×: position, type, ordnance, qty), damage thresholds, agility, bank/corner/acceleration |
| `.OT` | Static object | Hitpoints, flags, damage resistance, shape assignment |
| `.NT` | NPC / vehicle | Speed, turn rate, damage, hardpoints, AI params |
| `.JT` | Ordnance / weapon | Burst characteristics, guidance params, range, hit %, damage, projectile speed, firing arc |
| `.SEE` | Seeker / radar | Search/track lobes, cone angles, ranges, detection probability |
| `.ECM` | ECM suite/pod | Chaff/flare effectiveness, jamming effectiveness, power bitmask |
| `.GAS` | External fuel tank | Empty weight, fuel weight |

## Image Editing (PIC / PAL)

- Preview PIC files in the Preview panel (decoded via `fx::pic_decode`)
- Export PIC → PNG (uses the active preview palette, so the file matches the preview)
- Import PNG or BMP → PIC (dense format, full inline palette)
- Supports dense (format 0), sparse (format 1), and JPEG (format 0xD8FF) PIC sub-formats
- Covers aircraft skins, icons, nose art, tail art, and pilot portraits

### Palette viewer and switcher

- Opening any `.PAL` record (ICON.PAL, PALETTE.PAL, or a standalone file)
  shows a 16×16 swatch grid with per-index RGB tooltips and a **Use as
  preview palette** button
- A PIC's inline palette fragment appears as an **Inline palette**
  collapsible in the PIC editor
- The preview palette applies live to PIC previews (Preview panel combo) and
  CB8 frames (CB8 editor combo); one selection is shared by both
- **Auto** keeps the default behavior: PIC previews use PALETTE.PAL from any
  open session; CB8 renders greyscale, because the palette its videos expect
  is engine-internal and not stored in any LIB (PALETTE.PAL garbles it) —
  the switcher exists to experiment anyway
- Choosing a palette also drives PIC → PNG and CB8 frame exports; inline PIC
  palette fragments still overlay whichever base palette is selected

## Audio Editing (11K / 5K / 8K)

- Waveform display (downsampled to 512 points for performance)
- In-app playback via miniaudio with real-time position tracking
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

- Timeline table with fully editable rows: time (`N` absolute or `+N`
  relative), command (colored by type; free text — unknown commands stay
  editable), arguments, and a sync checkbox
- Add events: **+ Add Event** appends 100 ticks after the resolved timeline
  end; each row's **+** inserts after that row, inheriting its addressing
  mode (a `+1` neighbour after a relative row, so `+` chains keep resolving)
- Delete events with the row's **x** button
- Edited rows are rebuilt tab-separated per the SEQ layout and re-parsed
  through `fx::seq_parse`, so the table always shows what the codec sees;
  comment and untouched lines round-trip byte-identically via
  `fx::seq_serialize` (CRLF)

## Technical Info Editing (INF)

- **Styled** tab: each directive section (see [fa/formats/INF.md](fa/formats/INF.md) — plain text dot-commands, not RTF) rendered with its in-game alignment (`.left`/`.center`/`.right`) and title/body weight
- Per-section editing: text (Edit → Apply/Cancel), alignment and title/body style buttons, insert-after and delete, plus **+ Add Section**
- **Source** tab: the raw dot-command text in a scrollable editor; both tabs edit the same record
- Edited sections are recomposed via `fx::inf_rebuild_section` (CRLF, corpus blank-line convention); untouched sections keep their exact source bytes, so saves round-trip byte-identically outside the edit
- Save commits the INF bytes back into the LIB session

## Screenshot Viewer (RAW)

- Displays resolution and file size for `.RAW` screenshot files
- Decoded preview shown in the Preview panel via `fx::raw_decode`
- To export as PNG use `fx raw unpack <file>`

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

> Stats block (0xB0–0x0D7E) and campaign/inventory data: pending gameplay-derived saves (see [fa/formats/P.md](fa/formats/P.md) Open Questions).

## Settings / Preferences

All settings persist automatically in `fx-gui.ini` in the per-user preferences
directory (`~/.local/share/jomkz/fx-gui/` on Linux,
`%APPDATA%\jomkz\fx-gui\` on Windows) across restarts.

- **FA install directory** — set via File → Preferences; used by the one-click LIB install
- **Theme** — Auto (follow the system), Dark, or Light; set via File → Preferences
- **Recent files** — last 5 opened files; accessible from File → Recent Files; cleared from the same submenu
- **Window size and position** — restored on next launch; falls back to centered if the saved position is off-screen (position persistence is unavailable on Wayland)
- External tool integration: planned (Phase 3).

## Building

See [development.md](development.md).
