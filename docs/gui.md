# fxs — Graphical Editor

`fxs` is the interactive validation layer for the FA asset format research. It
exercises the `fx_lib` codecs against real game data and makes format behaviour
directly observable — loading a LIB archive, editing a type definition, or previewing
a decoded image confirms that the underlying format understanding is correct.

FATK (DuoSoft 1998), the original FA editor, is a 16-bit/32-bit VB6 application that
does not run on 64-bit Windows. `fxs` covers the same ground and more, but
replacing FATK is a byproduct rather than the goal.

## Platforms

`fxs` runs natively on Linux and Windows from the same code: SDL3 windowing with
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
  then mounts the LIBs' directory as a [workspace](#workspace) and cycles every
  icon-bar view (each category browser and Archives), opening the first object
  of each category — exercising extraction, every parser, the GPU upload paths,
  and the category browsers against real game data.
- **`--render <LIB> <ENTRY> [--out file.png] [--size WxH] [--software]`** —
  headless PNG snapshot: opens the archive, selects the entry (by 8.3 name
  like `A10.SH` or numeric index), settles the preview, and writes a PNG of
  the whole window through the same render path the interactive app uses.
  If the first argument is a **directory**, it is mounted as a workspace instead
  and `<ENTRY>` names the icon-bar view to capture (e.g. `aircraft`, `archives`;
  default Aircraft) — the headless way to review the category browsers.
  `--software` renders the SH preview through the FA-faithful software
  rasteriser (`fx_render::fa`, #290) instead of OpenGL — the headless way to
  produce software/GL side-by-side captures. Like
  `--smoke` it needs no visible window (offscreen fallback when there is no
  display server). `--out` defaults to `render.png`; `--size` defaults to the
  standard window size and is clamped to the minimum (the compositor may scale
  the actual pixel dimensions on HiDPI). Intended for automated visual review
  of rendering changes — SH 3D orbit, PIC/RAW/CB8 images, and the editors. The
  environment variable `FX_ARTIC="input=value"` (e.g. `_PLgearDown=1`) forces one
  SH articulation state before the capture, for reviewing a moving-part variant.

On Windows, `fxs.exe` is a `WIN32`-subsystem (GUI) binary, so shells launch
it detached: a bare `--smoke` invocation from PowerShell prints nothing,
returns immediately, and never sets `$LASTEXITCODE`, while the sweep runs
invisibly in the background. Pipe the output so the shell attaches stdout and
waits for the exit code:

```powershell
$fa = "C:\path\to\Fighters Anthology"
build\gui\Release\fxs.exe --smoke (Get-ChildItem "$fa\*.lib").FullName | Out-Host
echo $LASTEXITCODE   # expect 0, with one "swept" line per LIB
```

FA installs mix filename case (`FA_1.LIB`, `fa_7.lib`). `Get-ChildItem`
matches case-insensitively; a case-sensitive POSIX glob like `*.LIB` silently
misses the lowercase ones.

## Layout

Three-panel window:

| Panel | Contents |
|---|---|
| Left — Navigator | An [icon bar](#icon-navigation--category-browsers) selecting a category browser (named objects from the mounted [workspace](#workspace)) or the raw **Archives** LIB browser |
| Center — Editor | Form/text/timeline editor for the selected record |
| Right — Preview | Live preview: images (PIC with palette switcher, RAW screenshots), the SH 3D orbit view, and the T2 terrain viewer |

Menu bar: **File** · **View** · **Tools** · **Help**

| Menu | Items |
|---|---|
| File | Open LIB… (`Ctrl+L`), Open File… (`Ctrl+F`), Recent Files, Mount FA Workspace, Close / Close All, Preferences…, Exit |
| View | Expand All, Collapse All (LIB browser sessions) |
| Tools | Install `<session>` as FA_0.LIB |

## Library & Project Management

- Open FA / USNF97 / ATF Gold `.LIB` files; open loose files directly (BRF type records, PIC, RAW, audio, missions, SEQ, INF, SH, PLT, PAL)
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

## HUD & Sky Overlay Preview (HUD / LAY)

Both editors carry an in-game-style preview (#283) drawn through
`fx_render::fa` — the project's documented stand-in for the `G_*` raster
layer the engine's own HUD and horizon code draw through
([fa/hud.md](fa/hud.md), [fa/renderer.md](fa/renderer.md)) — alongside the
structural tables.

**HUD editor — Preview**

- Symbology positions come from the selected file's gauge parameters
  (flight-path marker anchor, heading/speed/altitude tapes, annunciators,
  readouts), so editing a `.HUD` moves the elements the way the engine
  would place them
- Flight state (heading/speed/altitude sliders, gear/flap/brake/hook and
  warning toggles) is simulated; the annunciators show the file's own
  advisory icon labels
- Text uses an install FNT resolved from the open sessions (the file's
  font asset prefixes, then `HUD*`/`4X6`); a built-in 4x6 block font is
  the fallback

**LAY editor — Sky Preview**

- Renders one layer's sky per its gradient ramps: the zenith→horizon ramp
  above the horizon line, the horizon-downward ramp below, each band
  Gouraud-interpolated between adjacent ramp entries — the
  `GouraudHorizon` palette-index banding
  ([fa/formats/LAY.md](fa/formats/LAY.md) § Engine Notes)
- The info line reports the documented per-angle band selection
  (`SetActiveLayerByAngle`) at level flight

**Fidelity boundary** (same spirit as the renderer's, see
[fa/renderer.md](fa/renderer.md) §3.1): element geometry inside each HUD
gauge (tick counts, label styling) and the HUD colour are preview
stand-ins — the engine takes them from `HUDInit`'s layout block, which is
not part of the `.HUD` file; only the per-element positions/sizes are the
file's. The LAY preview's angle-unit scale for band selection is inferred
(~256 units per quadrant fits every install file); the ramp order and
Gouraud banding are documented behaviour.

## 3D Model Viewer (SH)

- Selecting a `.SH` record shows a shaded 3D orbit view in the Preview panel,
  plus scale, vertex/face counts, bounding box, and the referenced texture names
  in the editor. Faces are drawn as **solid filled polygons** — the way FA
  renders shapes (span fills only, no edge pass; [fa/render-core.md](fa/render-core.md))
- **Orbit** by dragging; **zoom** with the scroll wheel; the camera auto-frames
  the model on selection
- The preview renders at the display's **physical** resolution, so it stays
  crisp (and the optional wireframe stays hairline) on HiDPI screens
- **Wireframe** toggle overlays a thin grey topology view — a validation aid,
  **off by default** so the preview matches FA's solid-fill rendering
- **Destroyed** toggle renders the damaged state: the inline damaged sub-model
  (`0xAC` JumpToDamage) when the shape carries one, else the whole-model wreck
  sibling (`<name>_A.SH`, resolved from the same LIB per
  [fa/shape-selection.md](fa/shape-selection.md) — the render-time swap the
  engine performs for destroyed aircraft; a `(wreck: …)` hint shows which
  sibling is displayed)
- Faces render with their **pre-shaded palette colour** (`ShFace::color`)
  directly, the way FA does — the model tool bakes the sun/orientation shading
  into the colour index (e.g. an aircraft walks a grey ramp face by face), so the
  preview applies **no** runtime relighting on top (an earlier dynamic light
  double-shaded those ramp entries to black)
- **Texture** toggle (shown when the model references a PIC that resolves in the
  same LIB) overlays the decoded skin on the textured faces. FA skins are
  texture **atlases** whose palette index `0xFF` is transparent
  ([fa/formats/PIC.md](fa/formats/PIC.md)): those texels show the face's flat
  colour through, exactly as FA composites them, rather than the atlas's unused
  black background. A face whose flat colour is index `0` (black) is a pure
  **decal overlay** (gear-pod stripes, panel markings): its transparent texels
  are see-through to the geometry behind, not filled — otherwise those panels
  render as solid black shapes. Both the OpenGL and FA-software backends honour
  this. Texture-swap damage (e.g. buildings) becomes visible here with
  **Destroyed** on
- **Frame** slider (shown only for animated models, i.e. `frame_count > 1`)
  selects the animation frame (`0x40` JumpToFrame); it drives `fx::ShState::frame`
  and re-parses on change. See [fa/formats/SH.md](fa/formats/SH.md#state-selected-rendering-read-codec)
- **LOD** slider (shown when the model has distance LODs, i.e. `lod_count > 1`)
  selects the level of detail (`0xC8` JumpToLOD): 0 = finest … coarsest; it
  drives `fx::ShState::lod`. The **Low detail** checkbox (shown when the model
  has a `0xA6` JumpToDetail switch) renders the low-detail preference blocks
  (`fx::ShState::detail = 0`)
- **Articulation combos** (one per moving-part input the shape exposes — **Gear**,
  **L Flap**, **R Flap**, **Airbrake**, **Rudder**, **Hook**, … from its x86
  `_PL*` selectors, #295). **All** merges every state (the codec default, which
  overlaps e.g. gear-up and gear-down geometry); picking a value emits just that
  one sub-stream (`fx::ShState::articulation`). The listed values are the shape's
  own compare cases (their per-shape meaning is documented in
  [fa/formats/SH.md](fa/formats/SH.md#x86unknown-region)); the continuous
  mid-travel animation is not reproduced — a chosen state renders at its authored
  position
- **Export OBJ…** writes a Wavefront OBJ (merges all state blocks; the selected
  frame/damage state is a preview-only choice, per the SH round-trip notes)
- **Software (FA)** switches the preview from OpenGL onto the FA-faithful
  software rasteriser (`fx_render::fa`, #290): indexed 16.16 spans, painter's
  order, no depth buffer — the pixel pipeline the game executable used.
  Colours quantize to the active preview palette (the stand-in for the
  engine's shade tables); geometry, spans, clipping, and occlusion are the
  faithful part. Same toggle headless via `--render … --software`

## Terrain Viewer (T2)

- Selecting a `.T2` record shows a textured 3D terrain — the leaf grid as a
  heightfield (elevation band → height), drawn through the shared
  [fx_render](fa/renderer.md) module like the SH viewer. Drag to orbit,
  scroll to zoom; a **Software (FA)** toggle switches to `fx_render::fa`.
- Each leaf is textured with its `texture_variant` tile
  (`<theater><N>.PIC`); water is drawn as flat sea. The tiles ship in a
  **sibling LIB** from the `.T2` (e.g. terrain textures in `FA_1.LIB`, the
  maps in `FA_2.LIB`), so open both — the viewer resolves tiles across every
  open session, and the status line shows the tile count. See
  [fa/formats/T2.md](fa/formats/T2.md#terrain-texturing) for the texturing
  model.
- **Fidelity boundary.** The terrain-band palette (indices 192–255, which
  the engine fills from the atmosphere/sky state) is a default earthy ramp
  here, and the orbit camera is a stand-in for the engine's VIEW-subsystem
  framing — the faithful camera is reproduced in fxe and consumed here
  ([#387](https://github.com/jomkz/fighters-codex/issues/387)). The mesh,
  per-leaf tile mapping, and the fx_render draw path are the faithful part.

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

All settings persist automatically in `fxs.ini` in the per-user preferences
directory (`~/.local/share/jomkz/fxs/` on Linux,
`%APPDATA%\jomkz\fxs\` on Windows) across restarts.

- **FA install directory** — set via File → Preferences; used by the one-click LIB install
- **Theme** — Auto (follow the system), Dark, or Light; set via File → Preferences
- **Recent files** — last 5 opened files; accessible from File → Recent Files; cleared from the same submenu
- **Window size and position** — restored on next launch; falls back to centered if the saved position is off-screen (position persistence is unavailable on Wayland)

## Workspace

**File → Mount FA Workspace** points fxs at an FA install and mounts the whole
directory as **one name-keyed namespace**, mirroring the engine's own startup
scan (`LibStartUp` builds a single sorted index over every LIB entry plus every
loose file — [memory-resource.md § LIB name resolution](fa/memory-resource.md#lib-name-resolution--the-hint-index)).
The root is the install directory set in **Preferences** (the same one Tools →
Install uses); mounting persists it, and fxs re-mounts it automatically on the
next launch. The status line reports what mounted, e.g.
`Mounted 10 LIBs + 44 loose files: 8531 names, 2 collisions`.

What is mounted:

- Every `*.LIB` in the root (case-insensitive — real installs mix `FA_2.LIB`
  and `fa_4c.lib`) is opened and its directory indexed.
- Every other file in the root is indexed as a loose entry.

**Name-collision precedence** follows the engine: a **LIB entry always resolves
before a loose file** of the same name, and within one kind the **later-mounted
source wins**. The engine's registration order is the operating system's
directory-enumeration order; fxs mounts LIBs in case-insensitive filename order
so the outcome is deterministic. Every collision is recorded and surfaced (the
status line count, and the workspace's collision list) rather than hidden — on a
full retail install only a couple of names clash, because each archive owns
distinct content ([LIB.md inventory](fa/formats/LIB.md#file-inventory)).

The workspace is the **data layer** for the object-category browsers; the raw
per-LIB **Archives** view (the LIB browser above) is unchanged and remains the
byte-level access path for validation work. (This "workspace" — a mounted install
root — is distinct from a "session," a single LIB opened for editing.)

### Asset-graph index

Once a workspace mounts, fxs builds an **asset-graph index** on a background
thread (the status line shows `Indexing assets... 6/10 archives` and never blocks
the UI). The index does two things.

**The cross-reference graph.** It parses each reference-bearing record and
resolves its links against the namespace: entity records (`.PT/.OT/.NT/.JT/.SEE/
.ECM/.GAS`, BRF text) name their shapes/sounds/HUD; a shape (`.SH`) names its
texture PICs and — via the engine's `_A`..`_D` wreck / `_S` shadow naming
(`sh_variant_name`) — its damage siblings; missions (`.M/.MM`) name their terrain
and object types; campaigns (`.CAM`) name their missions. Only links that resolve
to a real entry become edges, so display names and free text drop out.

**Categories.** Every entry is placed in **at least one** of eight buckets from
its type, with an explicit **Unassigned** bucket so nothing is hidden:

| Category | Types |
|----------|-------|
| Aircraft | `PT` |
| Vehicles | `NT`, `OT` |
| Weapons | `JT`, `SEE`, `ECM`, `GAS` |
| Missions | `M`, `MM`, `MT`, `MC` |
| Campaigns | `CAM`, `P` |
| Terrain | `T2`, `LAY` |
| Audio | `11K`, `5K`, `8K`, `22K`, `XMI`, `MUS` |
| Art/UI | `PIC`, `PAL`, `RAW`, `ICO`, `SH`, `HUD`, `FNT`, `DLG`, `MNU`, `PTS`, `HGR`, `SEQ`, `INF`, `AI`, `BI`, `CB8`, `VDO`, `FBC`, `TXT`, `WRI`, `HLP`, `CNT`, `INI`, `BIN`, `SMS`, `EXE` |
| Unassigned | anything else (loose `.DLL`, `.CFG`, `.DAT`, extension-less files…) |

On top of the type seed, an object's category **propagates along the graph** into
the art it reaches — so an aircraft's shape, its skin PIC and its wreck siblings
all also carry **Aircraft** and group with the `.PT` (e.g. `A10.PT` → `A10.SH` →
`_A10.PIC` + `A10_A.SH`). Propagation only crosses into Art/UI assets, so one
object type never bleeds into another (a `.PT` that lists a `.JT` weapon does not
make the weapon Aircraft). Categories are non-exclusive: a shape shared by an
aircraft and a vehicle carries both. These categories and the graph are what the
category browsers (below) and the object-scoped workspace view render.

### Icon navigation & category browsers

The left panel is topped by an **icon bar** ([generated icons](#object-category-icons))
with one button per category plus **Archives**. Selecting a category shows a
filterable browser of that category's **named objects** — the entries whose
primary type is that category (Aircraft lists the `.PT` aircraft, Weapons the
`.JT/.SEE/.ECM/.GAS`, and so on). An object's cluster art (its shapes and skin
PICs) is reached by opening the object, not by crowding the list, so it appears
under **Art/UI** and via the object's references rather than in every browser.
Selecting an entry opens it in the editor exactly like the Archives picker;
the current selection is remembered across category switches.

The **Archives** icon keeps today's raw per-LIB browser **unchanged** — the
byte-level tree of open LIB sessions, load-bearing for validation work. Category
browsers need a mounted workspace; until one is (or while it indexes) they show a
short prompt, and Archives is always available.

## Object-Category Icons

The object-centric navigation groups every asset under nine categories —
**Aircraft, Vehicles, Weapons, Missions, Campaigns, Terrain, Audio, Art/UI,**
and **Archives** (the raw per-LIB view). Their icons are **generated, committed
line-art** — no vendored icon font, in keeping with the zero-external-dependency
rule.

`tools/gen_icons.py` is the single source of truth. From one vector description
per icon it emits both:

- **theme-aware SVG sources** (`gui/assets/icons/*.svg`) using the same
  light/dark CSS recipe as the docs diagrams, for design review; and
- a **baked, zero-runtime-dependency form** fxs consumes
  (`gui/src/assets/icons_baked.{h,cpp}`): 8-bit coverage at two sizes (24 px and
  48 px for hidpi), anti-aliased by a stdlib rasteriser. The glyphs are
  monochrome, so only coverage is stored; fxs expands it to RGBA and **tints it
  with the active theme foreground** at draw time, so one baked artifact serves
  both light and dark.

Regenerate with `python3 tools/gen_icons.py`. A currency check
(`gen_icons.py --check`) runs on the CI docs job and as the `gen_icons_currency`
ctest (label `docs`), failing if a committed SVG or baked byte drifts from the
generator — the same guard the status matrix uses.

## Building

See [development.md](development.md).
