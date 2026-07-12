---
format: SSF
name: EA Installer Script
extensions: [".SSF"]
category: installer
endianness: none
spec:
  status: complete
codec:
  direction: round-trip
  byte_identical: true
  lib: [lib/src/ssf.cpp, lib/src/install.cpp]
  commands: [ssf, install]
  tests: [tests/test_ssf.cpp, tests/test_install.cpp]
  fuzz: [fuzz/fuzz_ssf.cpp, fuzz/fuzz_install.cpp]
  fixtures:
    synthetic: true
    real_manifest: false
related: [LIB, ESA, RGN]
---

# SSF — EA Installer Script (.SSF)

`.SSF` files are plain-text EA installer scripts that drive the FA
installation process. All three files reside in the Disc 1 root alongside the
installer executable. None are packed into any LIB archive.

## Tools

### fx

```
fx ssf info <file.SSF>     # keyword summary + round-trip check
fx ssf dump <file.SSF>     # every statement with its arguments
```

Line storage rides the shared line-preserving engine (`lib/src/txt.cpp`), so
any input round-trips byte-identically; keywords and their comma-separated
arguments are extracted as an overlay. Disc-1-only format (like
[RGN](RGN.md)), so the committed fixtures are synthetic; the three retail
scripts are exercised by running the install engine below against the mounted
discs.

Reading a script is half the story. Executing one is `fx install`, which
resolves these directives against the [ESA](ESA.md) archive and writes the
game to disk — see [Engine Notes](#engine-notes):

```
fx install plan   <disc-dir>… [-d dir] [--minimal] [--json]   # the dry run
fx install run    <disc-dir>… -d <dir> [--verify] [--overwrite]
fx install verify <disc-dir>… -d <dir>   # byte-compare the install to the disc
```

## File Layout

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

## File Inventory

| File | Role |
|------|------|
| `SETUP.SSF` | Master installer script — sets app metadata and references the two sub-scripts |
| `FINSTALL.SSF` | Full install — copies all assets including `FA_4B.LIB` (digital audio) |
| `MINSTALL.SSF` | Minimal install — omits `FA_4B.LIB`; music uses MIDI only |

### SETUP.SSF

Sets `COMPANY_NAME "Jane's Combat Simulations"`, `APP_NAME "Fighters
Anthology"`, `DEFAULT_PATH "\JANES\Fighters Anthology"`, then references both
sub-scripts:

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

**FA_3.LIB is absent from both manifests — it is CD-resident and loaded
directly from the disc at runtime.**

### Both scripts also install to the Windows system directory:

| File | Archive label | Destination |
|------|--------------|-------------|
| `EAREMOVE.EXE` | `REMOVER_EXECUTABLE_FILE` | `%WINDIR%\SYSTEM` |
| `EAEXEC.EXE` | `EXEC_EXECUTABLE_FILE` | `%WINDIR%\SYSTEM` |

These two are the EA installer's own uninstaller and launcher — the game loads
neither. They are the only entries in `SETUP.ESA` whose `flags` field is
`0x0221` rather than `0x0211` ([ESA § Entry flags](ESA.md#entry-flags--inferred)),
which is what identifies the field as a destination-class selector.

FA_4B.LIB contains the digital audio tracks (`.11K` music files). FA_4D.LIB
contains additional assets installed in both configurations.

### Files skipped during uninstall (SKIP_ON_REMOVE):

`*.P` (pilot files), `*.BKP` (pilot backups), `*.M` (mission files), `*.MT`
(mission text), `*.MM` (mission maps), `EA.CFG`, `MODEM.DAT`, `NET.DAT`,
`SERIAL.DAT`, `*.RAW` (screen captures), `*.GID` (WinHelp temp files)

## Engine Notes

`fx install` (`lib/src/install.cpp`) executes these scripts. It is what proves
the format is understood: a spec of a script language is a claim about what the
script *does*, and the only way to check that claim is to run it and compare the
result against a real installation.

A **disc is a directory** — an ISO mount, an extract of one, or the drive
itself. The engine is three stages, and only the ends touch a disk:

| Stage | | |
|-------|---|---|
| `install_scan` | I/O | directory → loose files + the ESA directory + the parsed scripts |
| `install_plan` | **pure** | → an `InstallPlan`: every file, every directive, every byte |
| `install_execute` / `install_verify` | I/O | write it; then byte-compare it back |

The middle stage is a pure function of scanned metadata, which is why the
planner is unit-tested and fuzzed with no media at all.

### What the engine does with each directive

**`INSTALL_FILES "glob","label","dest"`** selects every `SETUP.ESA` entry
carrying `label` whose name matches `glob`. The glob is DOS: `*.*` is
everything, `*.DLL` is a suffix, anything else is an exact (case-insensitive)
name. Three of the eleven directives use `*.*` — the label, not the glob, is
doing the selecting.

**Script choice is data-driven, not label-driven.** `SETUP.SSF` names its two
sub-scripts with localised prose (`":0409:Full Install - Digital Music:0C:…"`),
so nothing machine-readable says which is the full one. The engine resolves both
and takes the larger set. On the retail disc they differ by exactly one entry,
`FA_4B.LIB`.

**`INSTALL_SYSFILES`** is recorded and never performed: there is no Windows
system directory to write to, and the two files it names belong to the EA
installer rather than to FA.

**`SKIP_ON_REMOVE`** is documented as an *uninstall* hint, but it is also the
only statement in the scripts that says which files the **game** writes rather
than the installer — pilots (`*.P`), missions (`*.M`/`*.MT`/`*.MM`), `EA.CFG`,
screen captures (`*.RAW`). The engine reads it as a clobber guard: a file
matching one of these globs is never overwritten, *even with `--overwrite`*.
`EXAMPLE.MT` is the sharp case — the archive ships it and `*.MT` guards it, so a
fresh install writes it and a re-install keeps the one the user edited.

**The shell and registry directives** (`REGEXE`, `ADD_GROUP`, `GROUP_ITEM`,
`DESKTOP_ITEM`, `DIRECTX`) are reported as *not honored*, with a reason, rather
than silently dropped. On the retail `FINSTALL.SSF` — 33 statements — that is 8,
plus the 2 `INSTALL_SYSFILES`. An install that quietly ignores ten directives is
not a documented install.

### The CD-resident LIBs are a rule, not a list

`FA_4C.LIB` and `FA_7.LIB` (Disc 1) and all five Disc 2 LIBs appear in no
script: the game read them off the CD at run time. The engine copies **every
loose `*.LIB` in a disc root that the archive does not supply**, which yields
exactly those seven without a hard-coded manifest, and carries to the other EA
titles that share the format. Names are upper-cased on the way in, because the
case a disc shows is a property of the mount, not of the game (an ISO9660 mount
may hand back `fa_4c.lib`).

### What a full install is

Running both retail discs through the planner: **27 files, 1,036,798,285 bytes**
(989 MiB), of which the archive supplies 20 and the CD-resident rule 7. Two
entries are recorded and skipped (the `INSTALL_SYSFILES` pair), and
`PKCOMP.IDKDECODLL` — which no directive references — is never installed at all.
Payloads stream, so peak memory over a 989 MiB install is ~5 MB.

### Verified against a real installation

The engine's output was compared, file by file, against a licensed 1.02F install
of the game. Of the 19 files a minimal install writes, **14 are byte-identical**
and **4 differ — exactly the four the 1.02F patch rewrites** (`FA.EXE`,
`FA.SMS`, `FA_1.LIB`, `FA_2.LIB`). The 19th, `README.TXT`, that install does not
carry at all. Nothing else diverges: the scripts, the archive, and the glob
semantics are confirmed end to end, and the only divergence is the one the patch
explains. The disc is the **1.00F** build; see
[ESA § File Inventory](ESA.md#file-inventory) for what that means for the symbol
database, which describes 1.02F.

`fx install verify` re-derives each file from the disc and byte-compares it, so
it also reports a *legitimate* divergence: edit a mission and it will tell you
that `.MT` no longer matches the disc.

## Related

**Formats:** [LIB](LIB.md) — the archives the installer copies;
[ESA](ESA.md) — the archive the `INSTALL_FILES` labels select entries from, and
the other half of what `fx install` executes; [RGN](RGN.md) — installer UI
region maps (POSTER.RGN, BUTTONS.RGN) on Disc 1.
