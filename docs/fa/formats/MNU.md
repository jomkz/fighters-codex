---
format: MNU
name: Menu Screen Layout
extensions: [".MNU"]
category: ui-overlay
endianness: little
spec:
  status: stub
  gaps:
    - kind: re-static
      issue: 54
      note: "menu-tree/control layout encoding in the CODE section unmapped"
codec:
  direction: read
  rationale: "engine-code container (menu DLL) with the tree/layout encoding still unmapped (#54): fx_lib surfaces the container geometry and label strings the spec has confirmed; writing compiled menu DLLs is fighters-legacy territory"
  lib: [lib/src/mnu.cpp]
  commands: [mnu]
  tests: [tests/test_mnu.cpp]
  fuzz: [fuzz/fuzz_mnu.cpp]
  fixtures:
    synthetic: true
    real_manifest: true
    real_install: false
related: [DLG]
---

# MNU — Menu Screen Layout (.MNU)

FA_2.LIB contains 12 `.MNU` files. Each defines one top-level in-game menu
screen. All are **Win32 PE DLLs** (MZ stub + PE32 image) loaded by the FA
engine at runtime; they import rendering functions from `main.dll` (= the game executable —
see [architecture.md](../architecture.md#overlay-system--win32-pe-dlls)) and
embed their label strings directly in the PE data section.

## Tools

### fx

```
fx mnu info    <file.MNU>            # container check + CODE section geometry
fx mnu strings <file.MNU> [-n MIN]   # embedded menu label strings
```

Same MZ + Phar Lap `PL` container family as [CAM](CAM.md) (verified against
MAINMENU.MNU); the codec delegates to the shared PL reader.

## File Layout

All multi-byte integers are little-endian.

Win32 PE DLL (MZ DOS stub + PE32 image). All MNU files import rendering
functions from `main.dll` (= the game executable):

| Import | Role |
|--------|------|
| `_DrawAction` | Clickable action button |
| `_DrawRocker` | Toggle/rocker control |
| `_DrawText` | Static text label |

Label strings are embedded as null-terminated ASCII in the PE data section.
The record/layout encoding that arranges them into menu trees has not been
mapped — see Open Questions (the confirmed [DLG](DLG.md) dispatch-table
layout is the obvious starting hypothesis, since both formats share the
`_Draw*` import family).

## File Inventory

| File | Screen |
|------|--------|
| AR_MENU.MNU | Aircraft reference browser |
| ARMPLANE.MNU | Mission loadout / arm aircraft |
| CHOOSEM.MNU | Mission type selection + preferences |
| FMENUD.MNU | In-flight pause menu (full option tree) |
| MAINMENU.MNU | Minimal campaign action bar |
| MB_MENU.MNU | Mission briefing map controls |
| MC_MENU.MNU | Mission creator / editor |
| MULTI.MNU | Multiplayer lobby |
| QM_MENU.MNU | Quick mission setup |
| SELMENU.MNU | Aircraft / campaign selection bar |
| SM_MENU.MNU | Single mission aircraft filter |
| V_MENU.MNU | Vehicle reference browser |

All 12 live in FA_2.LIB.

### Menu Labels by File

**AR_MENU.MNU — Aircraft Reference Browser.** Object category filter:
**Fighters, Bombers, Helicopters, SAMs, Tanks, Ships, Other vehicles,
Structures, Missiles, Misc**. Pagination: **Next Page (PgDn), Prev Page
(PgUp)**. Display: **Show background in 3D view**, H3D Eyewear toggle, 3D
effect depth controls (Ctrl-=, Ctrl-[, Ctrl-]).

**ARMPLANE.MNU — Mission Loadout Screen.** **Weapons, Unload All, Cheat (load
anything anywhere)**. Navigation: **Airbase, Next Aircraft, Previous
Aircraft**. Campaign: **Campaign, Replay This Mission, Exit Campaign**.

**CHOOSEM.MNU — Mission Selection + Preferences.** Mission types: **Airbase
Assault** (plus others surfaced via DLG dialogs). Graphics prefs: **Screen
resolution** — 320×200, 640×480, 800×600, 1024×768. Network: **Serial, Modem,
IPX/SPX Network, TCP/IP Network, Disconnect**.

**FMENUD.MNU — In-Flight Menu.** **End mission** (Ctrl-Q), **Exit to Windows**
(Alt-F4).

- **Control:** Stick: Keyboard, Joystick, CH F-16 Flight Stick, CH F-16 Combat
  Stick, CH Flightstick Pro, Jane's Combat Stick, Microsoft Sidewinder 3D Pro.
  Rudder: Keyboard, Rudder pedals. Throttle: Keyboard, Throttle stick, Slews
  view, Vectors thrust.
- **Pref → Graphics:** HUD pitch ladder, Dim/Brighten HUD (Shift-[/]),
  cockpit, rear-view mirrors, large windows, authentic radar CRT, target info
  (Ctrl-T), IR/Laser targeting, radio silence (Alt-S).
- **Pref → Time:** Paused (Ctrl-P), Slow-motion (Shift-C), Accelerated time.
- **View:** Front, Back, Track, Player↔Missile, Player↔Wingman,
  Player↔Target, Target↔Player, Fly-by, External, Missile; View transitions
  toggle.
- **Window:** Envelope, Forward/IR-Laser (Shift-2), Other (Shift-3),
  Target/Radar (Shift-4/5), Navigation (Shift-6), System Status (Shift-7),
  Weapon Status (Shift-8), Radar (Shift-9), Radar Cross Section (Shift-0).
- **Cheat:** Damage level (Invulnerable/Normal/Realistic), Unlimited ammo,
  Unlimited fuel, Easy aiming, No crashes, No spins, No turbulence, Pull extra
  G, Ignore weapon weights, No sun/redout/blackout/screen-shake.
- **AI:** Enemy AI (Novice/Average/Unchanged), Ignore midair collisions, Easy
  targeting, Air combat guns only.
- **Show:** Planes, SAM sites, AAA sites, Ships, Airports, Vehicles, Other,
  SAM threat ranges; altitude presets (Ready for takeoff, Final approach,
  10,000 ft, 40,000 ft).
- **Multi:** Reduce bullet/missile accuracy/damage, reduce engine thrust/radar
  look-down, weapon camera, player scores, display windows, pauses flight.

**MAINMENU.MNU — Campaign Action Bar.** **Campaign, Replay This Mission, Exit
Campaign** — minimal bar overlaid on certain screens.

**MB_MENU.MNU — Mission Briefing Map.** Scroll (Left/Right/Up/Down), Center
map at cursor/selection, Zoom in/out/Smart zoom. **Waypoint:** Delete, Create
loop, Delete loop, Select prev/next waypoint. **Show:** Planes, SAM sites, AAA
sites, Ships, Airports, Vehicles, Other, Mission items only, SAM threat
ranges, Distance grid.

**MC_MENU.MNU — Mission Creator / Editor.** **File:** New mission (Ctrl-N),
Load mission, Save mission. **View:** same scroll/zoom controls as MB_MENU.
**World:** Set map, Set weather, Set friendly & enemy sides, Set screens;
Friendly/Enemy Pilot/SAM skill (Novice/Average/Good/Expert). **Object:**
Duplicate, Delete; Add/Remove from wing (Blue/Green/Black/White/Orange/
Purple/Yellow), Make wingleader; Add/Remove from group, Make groupleader.
**Waypoint:** same controls as MB_MENU. **Multiplayer:** Time limit, Number of
kills, End scenario conditions, Number of revives, Revive time delay, Revive
distance. **Aircraft era filter:** Fly All, 1956-1976, 1956-1982, 1956-1996,
1956-Future.

**QM_MENU.MNU / SM_MENU.MNU — Quick/Single Mission.** Aircraft era filter:
**Fly all, 1956-1976, 1956-1982, 1956-1996, 1956-Future**.

**SELMENU.MNU — Selection Bar.** **Cheat:** Allow Flying Any Plane.
**Campaign:** Replay This Mission, Exit Campaign.

**V_MENU.MNU — Vehicle Reference Browser.** Same category structure as
AR_MENU plus **Weapons** category.

## Open Questions

### 1. Menu-tree / control layout encoding

Only the import surface and the embedded label strings are mapped. The CODE
section structure that arranges labels into menu trees, binds shortcuts, and
positions controls is undecoded — the confirmed DLG dispatch-table record
format is the natural hypothesis to test first.

*Status: open — re-static (#54)*

## Related

**Formats:** [DLG](DLG.md) — dialog box overlays nested within menus; shares
the `_Draw*` import family and likely the record encoding.
