# Ghidra Scripts — FA.EXE Reverse Engineering

Scripts for decompiling and analysing Jane's Fighters Anthology (`FA.EXE`) using Ghidra.

The workbench is cross-platform: the analysis scripts (`*.java`) resolve every path
through the `FA_PROJECT` environment variable, the `.sh` launchers drive them on Linux
(the primary RE environment), and the `.bat` launchers remain for the Windows bench.

## Prerequisites (Linux)

| Tool | Version | Default path |
|---|---|---|
| [Ghidra](https://ghidra-sre.org/) | 12.1 | `~/tools/ghidra_12.1_PUBLIC` |
| JDK (with `javac`) | 21+ (Temurin 25 tested) | `~/tools/jdk-*` |
| FA game files | any FA install | `~/src/fa/game/` (`FA.EXE`, `FA.SMS`, `*.LIB`, overlay DLLs) |
| fx | this repo build | `build/{release,gcc,clang}/cli/fx` |

No root needed: unpack the Ghidra release zip and a JDK tarball under `~/tools/` and
`_env.sh` finds them. All launchers source [`_env.sh`](_env.sh), which resolves:

| Variable | Meaning | Default |
|---|---|---|
| `FA_PROJECT` | Ghidra project + output root | `~/src/fa` |
| `FA_INSTALL` | FA game files (`FA.EXE`, LIBs) | `$FA_PROJECT/game` |
| `GHIDRA_HOME` | Ghidra install | newest `~/tools/ghidra_*_PUBLIC`, else `/opt/ghidra*` |
| `JAVA_HOME` | JDK 21+ | newest `~/tools/jdk-*`, else system `javac` |

Export any of them before calling a launcher to override.

On Windows, edit the variables at the top of `run_ghidra.bat` instead
(`JAVA_HOME`, `GHIDRA_HOME`, `FA_PROJECT`; defaults `C:\java\jdk-26.0.1`,
`C:\tools\ghidra_12.1_PUBLIC`, `%USERPROFILE%\src\fa`).

---

## Quick start (automated)

```sh
scripts/ghidra/setup_project.sh     # create project + import FA.EXE + load FA.SMS symbols
scripts/ghidra/run_all.sh           # run all FA.EXE analysis scripts
scripts/ghidra/run_overlays.sh      # extract + import PE overlay DLLs
```

Windows bench: the same three steps exist as `setup_project.bat`, `run_all.bat`,
`run_overlays.bat`.

All output lands under `$FA_PROJECT/output/` and `$FA_PROJECT/overlay_projects/`.

---

## Migrating an existing project between machines

The analyzed Ghidra project is portable: copy `fa-re.gpr` and `fa-re.rep/` (and, if
wanted, `overlay_projects/` and `secondary_projects/`) into `$FA_PROJECT`. Two gotchas:

- **Owner check.** Ghidra records the OS user as project owner and refuses to open a
  project owned by anyone else — including a case difference (`John` on Windows vs
  `john` on Linux). Fix the owner in each migrated project:

  ```sh
  find "$FA_PROJECT" -path '*.rep/project.prp' \
      -exec sed -i 's/VALUE="John"/VALUE="john"/' {} +
  ```

- **Zero-byte `.gpr`.** The `.gpr` file is just a project marker in Ghidra 12.x; a
  0-byte `.gpr` is normal. All state lives in the `.rep/` directory.

---

## First-time setup (manual / GUI alternative)

### 1. Create the Ghidra project

1. Launch Ghidra and choose **File → New Project**
2. Create a **Non-Shared Project** at `~/src/fa`, name it `fa-re`
3. **File → Import File** → select `FA.EXE`
4. Accept the default PE import options and click **OK**
5. When prompted to analyse, click **Yes** and accept the default analysers

Auto-analysis takes a few minutes. Wait for it to finish before proceeding.

### 2. Import FA.SMS symbols

FA.SMS is the debug symbol table shipped with the game. Importing it names ~4 000 functions and globals, making decompiler output far more readable.

**Automated (headless):**
```sh
scripts/ghidra/run_ghidra.sh ImportFASmsHeadless.java
```
`ImportFASmsHeadless.java` resolves the SMS path automatically:
1. `-scriptArg` value if supplied
2. `$FA_PROJECT/FA.SMS`
3. `C:\JANES\Fighters Anthology\FA.SMS` (Windows fallback)

`setup_project.sh` passes `$FA_INSTALL/FA.SMS` explicitly, so the fallback chain only
matters when running the script by hand.

**GUI (interactive):**
1. In the Ghidra CodeBrowser, open `FA.EXE`
2. **Window → Script Manager**, locate `ImportFASms.java` and run it
3. When prompted, select `FA.SMS` from your FA install directory

> `ImportFASms.java` requires the Ghidra GUI. For automation use `ImportFASmsHeadless.java`.

### 3. Verify the project location

The headless scripts expect the project at:

```
$FA_PROJECT/fa-re.gpr
$FA_PROJECT/fa-re.rep/
```

If you placed it elsewhere, export `FA_PROJECT` (Linux) or update `FA_PROJECT` in
`run_ghidra.bat` (Windows).

---

## Running FA.EXE analysis scripts

**All subsystems — separate output files per script:**

```sh
scripts/ghidra/run_all.sh
```

Output: `$FA_PROJECT/output/Analyze*.txt`

**Consolidated single-file report:**

```sh
scripts/ghidra/run_ghidra.sh AnalyzeFA.java
```

Output: `$FA_PROJECT/output/AnalyzeFA.txt`

**Single subsystem:**

```sh
scripts/ghidra/run_ghidra.sh AnalyzeLAY.java
```

**First run with auto project setup:**

```sh
scripts/ghidra/run_all.sh --setup
```

---

## PE overlay DLL pipeline

FA stores many subsystems as Win32 PE DLLs packed inside `FA_2.LIB`. These have a Phar Lap signature (`PL\0\0`) instead of the standard `PE\0\0`. The overlay pipeline:

1. Unpacks `FA_1.LIB` and `FA_2.LIB` into a staging directory (`fx lib unpack`)
2. Sorts files by extension into `$FA_PROJECT/overlays/{BI,CAM,MC,HUD,LAY,FNT,MUS}`
3. Patches the two-byte signature `PL` → `PE` in each overlay (copies only — originals are preserved in `_all/`)
4. Imports each format group into its own Ghidra project under `$FA_PROJECT/overlay_projects/`

```sh
scripts/ghidra/run_overlays.sh              # full pipeline
scripts/ghidra/run_overlays.sh --extract    # extraction only
scripts/ghidra/run_overlays.sh --import     # import only (extract first)
scripts/ghidra/run_overlays.sh --import BI  # import single format
scripts/ghidra/run_overlays.sh --analyze    # DumpOverlayDLL over all overlay projects
scripts/ghidra/run_overlays.sh --secondary  # import secondary binaries
scripts/ghidra/run_overlays.sh --analyze-secondary  # DumpOverlayDLL over secondary projects
```

Secondary game binaries (IP.EXE, WAIL32.DLL, msapi.dll, CD-ROM DLLs) are copied to `$FA_PROJECT/overlays/secondary` and imported into `overlay_projects/secondary`; `--secondary` additionally imports each into its own isolated project under `$FA_PROJECT/secondary_projects/`.

**Format groups and counts:**

| Format | Count | Code? | Key unknowns |
|---|---|---|---|
| `.BI` | 9 | Yes (bytecode + native x86) | FRAME opcode 0x28 reader |
| `.CAM` | 6 | Yes (campaign state machine) | Full binary layout |
| `.MC` | 21 | Yes (condition evaluators) | Complete condition flow |
| `.HUD` | 46 | Data only (gauge tables) | Bit 14 writer at 0x4bc177/90 |
| `.LAY` | 24 | Data only (sky lookup tables) | Color-entry stride cross-check |
| `.FNT` | 15 | Data only (glyph bitmaps + dispatch) | No CLI extractor yet |
| `.MUS` | 9 | Data only (music bytecode) | All opcodes confirmed |

---

## Script inventory

### FA.EXE analysis scripts

| Script | Subsystem | Output |
|---|---|---|
| `AnalyzeFA.java` | Master — runs all subsystems | `AnalyzeFA.txt` |
| `AnalyzeLAY.java` | Sky / atmosphere / horizon | `AnalyzeLAY.txt` |
| `AnalyzeHUD.java` | HUD draw, warning bits, bit 14 SP writer | `AnalyzeHUD.txt` |
| `AnalyzeDLG.java` | Dialog / UI system | `AnalyzeDLG.txt` |
| `AnalyzePROJ.java` | Projectile / missile physics | `AnalyzePROJ.txt` |
| `AnalyzeSEE.java` | Seeker / missile guidance | `AnalyzeSEE.txt` |
| `AnalyzeMM.java` | Mission map / campaign | `AnalyzeMM.txt` |
| `AnalyzeBI.java` | BI bytecode interpreter / AI, FRAME opcode | `AnalyzeBI.txt` |
| `AnalyzeECM.java` | ECM / jammer | `AnalyzeECM.txt` |
| `AnalyzeHGR.java` | Hangar / airbase rendering | `AnalyzeHGR.txt` |
| `AnalyzeMUS.java` | Music / SEQ | `AnalyzeMUS.txt` |
| `AnalyzeOTNT.java` | Vehicle OT/NT classification, ot_flags gaps | `AnalyzeOTNT.txt` |
| `AnalyzeT2.java` | Terrain tile system | `AnalyzeT2.txt` |
| `AnalyzeGAS.java` | Fuel, hardpoints, BRF, JT physics offsets | `AnalyzeGAS.txt` |
| `AnalyzePT.java` | PT vehicle-record readers | `AnalyzePT.txt` |
| `AnalyzePLT.java` | Pilot file readers | `AnalyzePLT.txt` |
| `AnalyzeCAM.java` | Campaign DLL binary layout | `AnalyzeCAM.txt` |
| `AnalyzeMC.java` | Mission condition DLL flow | `AnalyzeMC.txt` |
| `AnalyzeT2DLL.java` | T2 terrain overlay DLL, surface-class mapping | `AnalyzeT2DLL.txt` |
| `AnalyzeGameLoop.java` | Main loop / frame dispatch | `AnalyzeGameLoop.txt` |
| `AnalyzeRenderer.java` | Software renderer internals | `AnalyzeRenderer.txt` |
| `AnalyzePhysics.java` | Flight model / physics core | `AnalyzePhysics.txt` |
| `AnalyzeNetwork.java` | Multiplayer / netcode | `AnalyzeNetwork.txt` |
| `AnalyzeInput.java` | Input / joystick handling | `AnalyzeInput.txt` |
| `DumpGlobals.java` | Global variable inventory | `DumpGlobals.csv` |
| `RecoverStructs.java` | Struct field recovery | `RecoverStructs.txt` |
| `DumpAllFunctions.java` | Full function dump | `DumpAllFunctions.txt` |

### Utility scripts

| Script | Purpose | Headless? |
|---|---|---|
| `FAScript.java` | Base class — shared helpers | n/a |
| `ImportFASms.java` | Import FA.SMS symbols (interactive file picker) | No |
| `ImportFASmsHeadless.java` | Import FA.SMS symbols (path from arg/env/default) | Yes |
| `DumpOverlayDLL.java` | Per-DLL dump for overlay/secondary projects | Yes |
| `AnalyzeCAMDLL.java` / `AnalyzeMCDLL.java` / `AnalyzeBIFRAME.java` | Deep dives on overlay projects | Yes |
| `AnalyzeSHHeader.java` | SH header field consumption — radius/radius_world evidence (#124) | Yes |

### Launchers (Linux `.sh` / Windows `.bat`)

| Linux | Windows | Purpose |
|---|---|---|
| `_env.sh` | — | Shared env resolution, sourced by every `.sh` launcher |
| `run_ghidra.sh` | `run_ghidra.bat` | Run a single analysis script against FA.EXE |
| `run_all.sh` | `run_all.bat` | Run all analysis scripts; `--setup` flag rebuilds the project first |
| `setup_project.sh` | `setup_project.bat` | One-shot: create project, import FA.EXE, load FA.SMS symbols |
| `extract_overlays.sh` | `extract_overlays.bat` | Unpack FA_1/FA_2.LIB and sort overlays by extension |
| `import_overlays.sh` | `import_overlays.bat` + `_import_one.bat` | Patch PL→PE signature and import overlay DLLs into Ghidra |
| `import_secondary.sh` | `import_secondary.bat` | Import secondary binaries into isolated projects |
| `run_overlays.sh` | `run_overlays.bat` + `_analyze_overlay*.bat`, `_analyze_secondary.bat` | Orchestrate extract / import / analyze; see flags above |

---

## Adding new scripts

Extend `FAScript` rather than `GhidraScript` directly — it provides all shared helpers and handles output file setup:

```java
public class AnalyzeMyThing extends FAScript {
    @Override
    public void run() throws Exception {
        openOutput("AnalyzeMyThing");  // writes to $FA_PROJECT/output/AnalyzeMyThing.txt

        header("My function (0x401000)");
        dumpAt(0x00401000L);

        header("Callers");
        dumpCallers(0x00401000L);

        closeOutput();  // prints "Output: <path>" automatically
    }
}
```

Keep scripts headless-compatible: no `askFile`, `askYesNo`, or `popup` calls.

Then add it to `run_all.sh` (and `run_all.bat`) and to `AnalyzeFA.java` if you want it in the consolidated report.
