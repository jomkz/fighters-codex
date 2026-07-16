# Shell / Menu / Dialog UI

The out-of-cockpit interface: the **menu-bar** state machine and the **dialog/widget**
core, the **text/format engine** (a dot-directive markup language shared by briefings,
reference pages, and the pilot dossier), the **name-list** machinery behind every file
picker, the **Jane's reference** (`INFO2*`) screens, and the **in-flight menu**
(`FlightMenu`).

> **Provenance:** Ghidra static analysis of the game executable with [FA.SMS](formats/SMS.md) symbols applied; recorded in the [symbol database](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/shell-ui.csv) and applied to the Ghidra project. Progress: [reconstruction matrix](reconstruction.md). Markers follow [spec-authoring.md](../spec-authoring.md): confirmed · inferred · unknown.

## Menu bar and dialogs

- **Menu core** (`0x40B8A0–0x40D79C`): the menu-bar state machine — `MenuStartUp`,
  `MenuLoadFont` (MENUFONT.PIC / MFONT320.PIC), `MenuMouseSelect` (hit-test), and
  `MenuDrawDropdown` (draw an opened submenu with background save/restore), over a
  `_firstMenu` linked list with palette-remap chrome.
- **Dialog/widget core** (`0x487A3A–0x48D200`): the DIALOG lifecycle — record hit-test and
  dispatch, and every `_Draw*` widget renderer (check/toggle/list-box/scrollbar) with
  edit/rocker/slider input. Many `_Draw*` handlers are label-only in FA.SMS and materialise
  on apply.
- **Shell chrome** (#492): `ShellSetup` loads the mouse-pointer PIC (`MOUSE320`/`MOUSEPTR`
  by resolution), the drop-shadow PIC set (`SHADV32`/`SHADV64` + corner pieces), and — for
  the in-flight menu — a steel background pattern and its palette band; `ShellOff` /
  `MenuShutDown` restore the saved background and palettes and free the chrome. `MainMenu`
  services the shell menu bar (Alt+F4 → exit; menu group 2 → `CampaignMenu`), and
  `DialogDrawBkgd` blits a named `.PIC` as a full-screen dialog backdrop. confirmed

![Shell UI: the menu-bar state machine and the dialog record hit-test/dispatch loop, each drawing through the rasterizer.](diagrams/shell-ui.svg)

## The text/format engine (#492)

`0x47D190–0x47F660` is a self-contained rich-text engine. `PrepareText(text, len, section,
style, x, y, w, h, …)` paginates a marked-up buffer into 0x18-byte page records (font,
indent state, text cursor per page); `PrintText(page)` replays one page; `PageText`,
`FreeTextStuff` (pop one nested context; up to four contexts stack at `_fmtText`),
`FormatInit`, and `EndText` manage the lifecycle. Fonts load by index through a
`TITLEFONT.PIC`-rooted name table with cached handles (`FormatLoadFont`); glyph metrics
come from a 6-byte-per-glyph table at font-PIC `+0x2A` (width `+2`, height `+4` —
`FontGlyphSize`). confirmed

**The directive language.** `FormatToken` (`0x47D700`) walks the text one whitespace
token at a time; dot-directives dispatch to `FormatDirective` (`0x47E1B0`) — the exact
compare chain the [MT](formats/MT.md) spec's directive table was read from (`.section`,
`.page`, `.title`/`.header`/`.body`, `.italic`/`.bold`/`.underline` with `..` off forms,
`.left`/`.right`/`.center`/`.full`, `.indent_*`, `.picture`, `.sound`, `.music`/
`.music_off`, `.button`/`.dbutton` → hit boxes in `_buttonBoxes`/`InTextButton`) — and
plain words are measured, word-wrapped, and rendered. `FindSection`/`FindSectionHeader`
locate `.section <n>` entry points. confirmed

The one engine renders the [`.MT`](formats/MT.md) briefing texts, the reference `.INF`
pages, `SHWPILOT.TXT` (the pilot dossier template), the `.CAM` campaign descriptions
(`<name>.TXT`), and inline UI strings (`PilotScreen` lays out its Prev/Next strip as a
`.button` string). `AddStats` composes the whole debrief — CAMPAIGN / MISSION AVERAGES /
PLAYER WINGMAN / AIRBASE sections with kills, losses, damage, landing grade — as
directive-marked text into this engine. `_GetString` (modal edit dialog) and `ClipString`
(ellipsized print) round out the text utilities. confirmed

## Name lists (#492)

`GetNames(mask)` (`0x41C840`) builds the list behind every picker: category bits select
directory globs — `types\*.PT`/`.NT`/`.OT`/`.JT`, `mission\*.M`, `*.T2` (theaters),
`*.2D`, `*.P` (pilots), `*.COM` (with the DOS sound drivers `SOUNDRV`/`MIDPAK`/`CMIDPAK`/
`RMIDPAK` excluded) — into 0x30-byte NAMES records `{u32 class/flags, char[13] filename,
char[30] title}`. Titles come from the mission-file header or the type record; type
entries carry the class byte, the user-flyable flag (type `+9` bit `0x4000`), and an **era
filter on the type-record year at `+0x37`** (the reference screen's era categories, e.g.
1955–1976). `NamesSort`/`NamesDedupe` (quicksort by title + adjacent-filename compaction),
`MakeNamesForList` (NUL-separated buffer for the DLG list widget), and `DialogPickFiles`
(the generic "Choose an object/mission/map" picker into `_itemPicked`) sit on top.
confirmed

## The Jane's reference screens (`INFO2*`, #492)

`INFO2Screen` (`0x461710`) is the aircraft/weapons encyclopaedia: `ar_menu` + `ar_dlg`
over an `ar` background, a `day1.LAY` weather context for the 3-D pane, and `GetNames`
category filters on the Reference menu. `INFO2SetType` selects an item — loading its type
record, sizing the 3-D view from `ObjRadius × 15`, probing `<type>_0…8` photo pages, and
enabling the media buttons. `INFO2Draw` renders the pane by mode: confirmed

- **Text** — the type's `.INF` file through the format engine, paginated.
- **3-D view** — a two-entry display list (horizon + shape) rendered by `GRRender` with
  the shape's own palette band, a `v_air`/`v_land`/`v_sea` backdrop chosen from the type's
  basing flags, arrow-key/mouse-drag rotation (5°/step) and +/− zoom. The four carriers
  (`nimz`/`wasp`/`kitt`/`clem.NT`) substitute extended display shapes (`x<name>.SH`).
  With the 3-D-glasses pref (`gamePrefs` bit `0x20000000`) it renders a **stereo pair**
  offset by `±_glassesXDiff` and interleaves the halves (`GLASSES*`).
- **Photos/art** — `<type>_<n>.PIC` pages plus fixed `_p`/`_c`/`_e`/`_f` art, with
  `ar_nopic` as the fallback and `PAGE n OF m` overlay.
- **Video** — three per-type videos (`ar_vidp`/`ar_vidh`/`ar_vidm` stills when present)
  played through the Cobra player (`PlayCobra`; see [video-decode.md](video-decode.md)).

The **Fly** button (flyable types, or `ASTOVL`) writes `_missionName = ~a<type>.M`
(non-flyable: `~info.M`) and `_selectedTypeName`, then returns 1 — the shell launches a
generated quick mission. The `~` filename prefix marks such engine-generated missions
(`~fake.M`, `~info.M` — the corpus-prefix question from the asset audit). confirmed

## FlightMenu — the in-flight menu (#492)

`FlightMenu` (`0x474800`) pauses the sim (`_timeCompression = 0x7FFF`) and presents the
FMENUD menu; every item maps onto engine state, which makes it the index of the
preference bits: confirmed

- **Control** (group `0x200`): `_stickDevice` 0–6 / `_rudderDevice` 7–8 /
  `_throttleDevice` 9–10 (the [device-proc table](input.md#the-control-device-layer)),
  each followed by `InputCalibrate`; coolie-hat mode (`gameMultiPrefs` bit `0x800`:
  view slew vs thrust vector).
- **Sim** (group `0x300`): time compression (paused `0x7FFF`; −1, 0…3 as shift counts =
  1/2x, 1x-8x; capped at 1× in multiplayer), `GraphicPrefs`/`SoundPrefs` dialogs, HUD
  brightness, cockpit style (`gamePrefs` bit `0x800000`, rebuilds via `CPShutdown`/
  `CPInit`), realistic avionics (bit `0x2000000`), radio silence (bit `0x100000`), and
  more single-bit toggles.
- **Views** (group `0x400`): `VIEWBuild` with the key code of the view key — literally
  `0x3B00`–`0x4400` (F1–F10) and `0x5800` (F12) — plus padlock (bit `0x8000`) and the
  3-D-glasses items.
- **Windows** (group `0x500`): `CPToggleWindow` 1–11, mirroring Shift+digit.
- **Difficulty** (group `0x600`): a 3-bit difficulty field in the low `gamePrefs` bits
  (changed only when landed), invulnerability and the other gameplay toggles; disabled
  for non-host players in multiplayer.
- **Multiplayer** (group `0x700`) and **free flight** (group `0x800`): `gameMultiPrefs`
  bits, the AI-skill confirmation dialogs (`CPSetSkill`), in-flight-map (`__ifmShow`)
  overlay masks — or, in free flight, `APTeleport` airport teleports.

Exit writes the config (`WriteConfig`), fires `MISSIONPrefsChanged` when a pref changed,
and re-broadcasts prefs in multiplayer. The user-facing item labels live in the FMENUD
`.MNU` resource — matching each bit to its label is an asset read (see Open Questions).

## Functions

Full record: [`db/symbols/shell-ui.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/shell-ui.csv).

| VA | Symbol | Role |
|----|--------|------|
| `0x40BD30` | `MenuStartUp` | initialise the menu bar |
| `0x40C670` | `MenuMouseSelect` | hit-test the mouse over the bar/items |
| `0x40BA10` | `ShellSetup` | shell chrome: pointer, shadows, steel pattern |
| `0x41C840` | `GetNames` | build the NAMES list for a category mask |
| `0x461710` | `INFO2Screen` | the Jane's reference screen loop |
| `0x45FEC0` | `INFO2Draw` | reference pane renderer (text/3-D/photo/video) |
| `0x474800` | `FlightMenu` | the in-flight menu |
| `0x47D440` | `PrepareText` | paginate marked-up text |
| `0x47D700` | `FormatToken` | the `[tag]` markup interpreter |
| `0x4A18A0` | `CampaignSelect` | campaign chooser over `.CAM` + `<name>.TXT` |
| `0x4A1C80` | `DialogPickFiles` | generic object/mission/map picker |
| `0x4A2220` | `GraphicPrefs` | graphics preferences dialog |

## Open Questions

### 1. DLG record types — mapped

The `.DLG` record types 1/3/4/5/7/8 are now field-mapped in [DLG.md](formats/DLG.md) § Per-type
record fields (recovered under [#258](https://github.com/jomkz/fighters-codex/issues/258) from
`DialogUpdate`, `_DrawListBox`, and the scrollbar helpers). Only a few engine-scratch interior
bytes of the largest records remain unnamed (tracked as the residual `.DLG` gap under #54).

*Status: resolved — re-static (#258; residual scratch fields under #54).*

### 2. Menu/preference labels are in the assets

`FlightMenu` and `INFO2Screen` map menu-item ids to `gamePrefs`/`gameMultiPrefs` bits, but
the user-facing labels live in the `.MNU` resources (FMENUD, `ar_menu`, MAINMENU). Reading
those assets would name every preference bit outright — cheaper than any further static
analysis.

*Status: open — re-asset (parse the retail `.MNU` items with the [MNU](formats/MNU.md) codec and join on item id).*

### 3. The FA.SMS labels `PilotSetField`/`PilotFieldProc` sit on libjpeg code

The bodies at `0x468F80`/`0x469010` are the **libjpeg decompression post-processing
controller** (`jinit_d_post_controller`/`start_pass_dpost` — pool alloc, `jround_up`,
pass-mode function-pointer selection) — not pilot code. FA links a (C++-compiled) libjpeg;
`jdiv_round_up`/`jcopy_sample_rows` and friends sit at `0x476060–0x476120` next to
`MainWndproc`. The two mislabeled VAs are left **unclaimed** rather than propagate a wrong
name; the JPEG cluster most plausibly serves the Cobra video decoder and should be homed
(and the labels corrected) with the video subsystem.

*Status: open — re-static (home the vendored libjpeg cluster; correct the two FA.SMS labels; see video-decode.md).*

### 4. `*.COM` and `*.2D` name-list categories

`GetNames` has category globs for `*.COM` (excluding the DOS sound-driver COMs) and
`*.2D` whose consumers haven't been traced — likely legacy-import listings. The `*.P`
category is the pilot roster; `*.T2` is the theater list for the mission creator.

*Status: open — re-static (find the callers passing masks 0x80000/0x10000).*

## Related

- [formats/DLG.md](formats/DLG.md) / [formats/MNU.md](formats/MNU.md) — the dialog and menu formats.
- [formats/MT.md](formats/MT.md) — the briefing text the format engine renders.
- [input.md](input.md) — the key-code space and the device procs the menu configures.
- [renderer.md](renderer.md) — the `G_*` rasterizer the UI draws through.
- [campaign.md](campaign.md) — the campaign/mission/pilot screens the shell hosts.
- [video-decode.md](video-decode.md) — the Cobra player behind the reference videos.
