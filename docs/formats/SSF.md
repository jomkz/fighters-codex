# EA Installer Script (.SSF)

`.SSF` files are plain-text EA installer scripts that drive the FA installation process. All three files reside in the Disc 1 root alongside the installer executable. None are packed into any LIB archive.

## Location

Disc 1 (E:) root. Not packed into any LIB archive.

## Known Files

| File | Role |
|------|------|
| `SETUP.SSF` | Master installer script — sets app metadata and references the two sub-scripts |
| `FINSTALL.SSF` | Full install — copies all assets including `FA_4B.LIB` (digital audio) |
| `MINSTALL.SSF` | Minimal install — omits `FA_4B.LIB`; music uses MIDI only |

## Grammar (Confirmed)

Plain ASCII text. `#` begins a comment. Keywords are all-caps.

| Keyword | Arguments | Effect |
|---------|-----------|--------|
| `COMPANY_NAME` | `"string"` | Sets company name string (used in registry keys and Start Menu path) |
| `APP_NAME` | `"string"` | Sets application name string |
| `DEFAULT_PATH` | `"\path"` | Suggested install path (no drive letter; installer picks drive) |
| `INSTALL_SCRIPT` | `"file.SSF", "label"` | References a sub-script; label may embed locale tags (`":0409:English:0C:French"`) |
| `CREATE_FOLDERS` | `"[INSTALL_PATH]"` | Creates the install directory |
| `INSTALL_FILES` | `"glob", "archive_label", "dest"` | Copies files matching glob from named archive to destination |
| `INSTALL_SYSFILES` | `"filename", "archive_label"` | Copies a file to `%WINDIR%\SYSTEM` |
| `SKIP_ON_REMOVE` | `"glob"` | Marks files to skip deletion during uninstall |
| `REGEXE` | `"path\app.exe"` | Registers executable with the OS |
| `ADD_GROUP` | `"Company\App"` | Creates a Start Menu program group |
| `GROUP_ITEM` | `"group\name","path\exe"` | Adds a Start Menu shortcut |
| `DESKTOP_ITEM` | `"name","path\exe"` | Adds a desktop shortcut |
| `DIRECTX` | `"label",major,minor` | Invokes DirectX component install |

## File Manifest (Confirmed)

### SETUP.SSF
Sets `COMPANY_NAME "Jane's Combat Simulations"`, `APP_NAME "Fighters Anthology"`, `DEFAULT_PATH "\JANES\Fighters Anthology"`, then references both sub-scripts:
```
INSTALL_SCRIPT "MINSTALL.SSF", ":0409:Minimal Install - Midi Music:…"
INSTALL_SCRIPT "FINSTALL.SSF", ":0409:Full Install - Digital Music:…"
```

### Both FINSTALL.SSF and MINSTALL.SSF install:

| File/Glob | Archive label | Destination |
|-----------|--------------|-------------|
| `FA.EXE` | `FA_EXECUTABLE_FILES` | `[INSTALL_PATH]` |
| `FA.SMS` | `FA_EXECUTABLE_FILES` | `[INSTALL_PATH]` |
| `*.*` | `FA_README` | `[INSTALL_PATH]` |
| `*.*` | `FA_INTERNET` | `[INSTALL_PATH]` |
| `FA_1.LIB` | `FA_LIBS` | `[INSTALL_PATH]` |
| `*.*` | `FA_MISC` | `[INSTALL_PATH]` |
| `FA_2.LIB` | `FA_LIBS` | `[INSTALL_PATH]` |
| `FA_4D.LIB` | `FA_LIBS` | `[INSTALL_PATH]` |
| `WAIL32.DLL` | `FA_SOUND_DRIVER_FILES` | `[INSTALL_PATH]` |
| `*.DLL` | `COMMDRV_DLLS_FILES` | `[INSTALL_PATH]` |

### FINSTALL.SSF additionally installs:

| File/Glob | Archive label | Destination |
|-----------|--------------|-------------|
| `FA_4B.LIB` | `FA_LIBS` | `[INSTALL_PATH]` |

**FA_3.LIB is absent from both manifests — it is CD-resident and loaded directly from the disc at runtime.**

FA_4B.LIB contains the digital audio tracks (`.11K` music files). FA_4D.LIB contains additional assets installed in both configurations.

### Files skipped during uninstall (SKIP_ON_REMOVE):

`*.P` (pilot files), `*.BKP` (pilot backups), `*.M` (mission files), `*.MT` (mission text), `*.MM` (mission maps), `EA.CFG`, `MODEM.DAT`, `NET.DAT`, `SERIAL.DAT`, `*.RAW` (screen captures), `*.GID` (WinHelp temp files)

## Related

- [EALIB.md](EALIB.md) — LIB archive format
- [RGN.md](RGN.md) — installer UI region maps (POSTER.RGN, BUTTONS.RGN) on Disc 1
