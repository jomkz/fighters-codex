# Ghidra Scripts — FA.EXE Reverse Engineering

Scripts for decompiling and analysing Jane's Fighters Anthology (`FA.EXE`) using Ghidra.

## Prerequisites

| Tool | Version | Default path |
|---|---|---|
| [Ghidra](https://ghidra-sre.org/) | 12.1 | `C:\tools\ghidra_12.1_PUBLIC` |
| JDK | 21+ (JDK 26 tested) | `C:\java\jdk-26.0.1` |
| FA.EXE | any FA install | `C:\JANES\Fighters Anthology\FA.EXE` |
| FA.SMS | same FA install | `C:\JANES\Fighters Anthology\FA.SMS` |
| ft.exe | this repo build | `build\cli\Release\ft.exe` |

Edit the variables at the top of `run_ghidra.bat` if your Ghidra / JDK locations differ:

```bat
set JAVA_HOME=C:\java\jdk-26.0.1
set GHIDRA_HOME=C:\tools\ghidra_12.1_PUBLIC
set FA_PROJECT=%USERPROFILE%\src\fa
```

---

## Quick start (automated)

```bat
scripts\ghidra\setup_project.bat     # create project + import FA.EXE + load FA.SMS symbols
scripts\ghidra\run_all.bat           # run all 17 FA.EXE analysis scripts
scripts\ghidra\run_overlays.bat      # extract + import PE overlay DLLs
```

All output lands under `%FA_PROJECT%\output\` and `%FA_PROJECT%\overlay_projects\`.

---

## First-time setup (manual / GUI alternative)

### 1. Create the Ghidra project

1. Launch Ghidra and choose **File → New Project**
2. Create a **Non-Shared Project** at `%USERPROFILE%\src\fa`, name it `fa-re`
3. **File → Import File** → select `FA.EXE`
4. Accept the default PE import options and click **OK**
5. When prompted to analyse, click **Yes** and accept the default analysers

Auto-analysis takes a few minutes. Wait for it to finish before proceeding.

### 2. Import FA.SMS symbols

FA.SMS is the debug symbol table shipped with the game. Importing it names ~4 000 functions and globals, making decompiler output far more readable.

**Automated (headless):**
```bat
scripts\ghidra\run_ghidra.bat ImportFASmsHeadless.java
```
`ImportFASmsHeadless.java` resolves the SMS path automatically:
1. `-scriptArg` value if supplied
2. `%FA_PROJECT%\FA.SMS`
3. `C:\JANES\Fighters Anthology\FA.SMS` (fallback)

**GUI (interactive):**
1. In the Ghidra CodeBrowser, open `FA.EXE`
2. **Window → Script Manager**, locate `ImportFASms.java` and run it
3. When prompted, select `FA.SMS` from your FA install directory

> `ImportFASms.java` requires the Ghidra GUI. For automation use `ImportFASmsHeadless.java`.

### 3. Verify the project location

The headless scripts expect the project at:

```
%USERPROFILE%\src\fa\fa-re.gpr
%USERPROFILE%\src\fa\fa-re.rep\
```

If you placed it elsewhere, update `FA_PROJECT` in `run_ghidra.bat`.

---

## Running FA.EXE analysis scripts

**All subsystems — separate output files per script:**

```bat
scripts\ghidra\run_all.bat
```

Output: `%FA_PROJECT%\output\Analyze*.txt` (17 files)

**Consolidated single-file report:**

```bat
scripts\ghidra\run_ghidra.bat AnalyzeFA.java
```

Output: `%FA_PROJECT%\output\AnalyzeFA.txt`

**Single subsystem:**

```bat
scripts\ghidra\run_ghidra.bat AnalyzeLAY.java
```

**First run with auto project setup:**

```bat
scripts\ghidra\run_all.bat --setup
```

---

## PE overlay DLL pipeline

FA stores many subsystems as Win32 PE DLLs packed inside `FA_2.LIB`. These have a Phar Lap signature (`PL\0\0`) instead of the standard `PE\0\0`. The overlay pipeline:

1. Unpacks `FA_2.LIB` into a staging directory
2. Sorts files by extension into `%FA_PROJECT%\overlays\{BI,CAM,MC,HUD,LAY,FNT,MUS}`
3. Patches the two-byte signature `PL` → `PE` in each overlay (copies only — originals are preserved in `_all\`)
4. Imports each format group into its own Ghidra project under `%FA_PROJECT%\overlay_projects\`

```bat
scripts\ghidra\run_overlays.bat              # full pipeline
scripts\ghidra\run_overlays.bat --extract    # extraction only
scripts\ghidra\run_overlays.bat --import     # import only (extract first)
scripts\ghidra\run_overlays.bat --import BI  # import single format
```

Secondary game binaries (IP.EXE, WAIL32.DLL, msapi.dll, CD-ROM DLLs) are copied to `%FA_PROJECT%\overlays\secondary` and imported into `overlay_projects\secondary`.

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
| `AnalyzeCAM.java` | Campaign DLL binary layout | `AnalyzeCAM.txt` |
| `AnalyzeMC.java` | Mission condition DLL flow | `AnalyzeMC.txt` |
| `AnalyzeT2DLL.java` | T2 terrain overlay DLL, surface-class mapping | `AnalyzeT2DLL.txt` |

### Utility scripts

| Script | Purpose | Headless? |
|---|---|---|
| `FAScript.java` | Base class — shared helpers | n/a |
| `ImportFASms.java` | Import FA.SMS symbols (interactive file picker) | No |
| `ImportFASmsHeadless.java` | Import FA.SMS symbols (path from arg/env/default) | Yes |

### Batch launchers

| Script | Purpose |
|---|---|
| `run_ghidra.bat` | Run a single analysis script against FA.EXE |
| `run_all.bat` | Run all 17 analysis scripts; `--setup` flag rebuilds the project first |
| `setup_project.bat` | One-shot: create project, import FA.EXE, load FA.SMS symbols |
| `extract_overlays.bat` | Unpack FA_2.LIB and sort overlays by extension |
| `import_overlays.bat` | Patch PL→PE signature and import overlay DLLs into Ghidra |
| `run_overlays.bat` | Orchestrate extract + import; supports `--extract`/`--import [FORMAT]` |

---

## Adding new scripts

Extend `FAScript` rather than `GhidraScript` directly — it provides all shared helpers and handles output file setup:

```java
public class AnalyzeMyThing extends FAScript {
    @Override
    public void run() throws Exception {
        openOutput("AnalyzeMyThing");  // writes to %FA_PROJECT%\output\AnalyzeMyThing.txt

        header("My function (0x401000)");
        dumpAt(0x00401000L);

        header("Callers");
        dumpCallers(0x00401000L);

        closeOutput();  // prints "Output: <path>" automatically
    }
}
```

Keep scripts headless-compatible: no `askFile`, `askYesNo`, or `popup` calls.

Then add it to `run_all.bat` and to `AnalyzeFA.java` if you want it in the consolidated report.
