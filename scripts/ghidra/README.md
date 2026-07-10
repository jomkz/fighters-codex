# Ghidra Scripts — Fighters Anthology Reverse Engineering

Scripts for decompiling and analysing Jane's Fighters Anthology using Ghidra — `FA.EXE`
(epic #209) and the companion binaries it ships alongside (IP.EXE, WAIL32.DLL, comms DLLs;
epic #247).

The workbench runs on **Linux** (the primary RE environment since #120): the analysis
scripts (`*.java`) resolve every path through the `FA_PROJECT` environment variable, and the
`.sh` launchers drive them. The frozen Windows `.bat` mirror suite was retired in #374 — if a
Windows bench is ever needed again, recover the mirrors from git history (the `.java` scripts
themselves are already OS-agnostic).

**Per-binary reconstruction:** the symbol-DB launchers (`apply_symbols`, `apply_types`,
`export_inventory`, `rebuild_audit`) take an optional `BINARY` argument — the Ghidra program
name = imported filename (default `FA.EXE`; `ALL` loops every binary in `db/subsystems.csv`).
Each targets one image via `-process <BINARY>`, and inventory exports to
`db/inventory/<BINARY>/` — VAs are unique only within a binary (IP.EXE bases at the same
`0x00400000` as FA.EXE). Import the companion binaries first with `import_targets.sh`
(they are standard PE, so no Phar Lap `PL` patch — unlike the LIB-embedded data overlays).

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
| `GHIDRA_HEADLESS_MAXMEM` | headless JVM heap | `8G` (Ghidra's own default is only `2G`) |

Export any of them before calling a launcher to override.

### Core utilization

`DumpAllFunctions` decompiles the whole image with
[`ParallelDecompiler`](DumpAllFunctions.java) — one native decompiler process
per core — instead of a single serial `DecompInterface` (which `FAScript` keeps
for the targeted `Analyze*` scripts that dump only a handful of functions each).
Ghidra auto-detects the core count for its thread pools; the launchers set no
`-max-cpu` and no `-Dcpu.core.limit`, so nothing caps it. To throttle on a
shared box, export `GHIDRA_HEADLESS_JAVA_OPTIONS="-Dcpu.core.limit=<n>"`.

FA.EXE is small and its functions are mostly tiny, so the whole-image decompile
is only a few seconds and per-invocation JVM+project-load overhead dominates a
single run (≈14 s wall for all 2.8k functions here). The remaining serialization
is `run_all.sh` invoking each analysis script in its own cold Ghidra process;
parallelizing those is gated on read-only project sharing and is left as a
follow-up rather than risk write-lock contention on the shared project.

---

## Quick start (automated)

```sh
scripts/ghidra/setup_project.sh     # create project + import FA.EXE + load FA.SMS symbols
scripts/ghidra/run_all.sh           # run all FA.EXE analysis scripts
scripts/ghidra/run_overlays.sh      # extract + import PE overlay DLLs
```

All output lands under `$FA_PROJECT/output/` and `$FA_PROJECT/overlay_projects/`.

---

## Migrating an existing project between machines

The analyzed Ghidra project is portable: copy `fa-re.gpr` and `fa-re.rep/` (and, if
wanted, `overlay_projects/`) into `$FA_PROJECT`. Two gotchas:

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
2. `$FA_PROJECT/FA.SMS` (falling back to `~/src/fa/FA.SMS`)

`setup_project.sh` passes `$FA_INSTALL/FA.SMS` explicitly, so the fallback chain only
matters when running the script by hand. (The GUI-only `ImportFASms.java` was retired in
#374 — the headless variant covers every use; run it interactively from the Script Manager
if you must.)

### 3. Verify the project location

The headless scripts expect the project at:

```
$FA_PROJECT/fa-re.gpr
$FA_PROJECT/fa-re.rep/
```

If you placed it elsewhere, export `FA_PROJECT` before calling a launcher.

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
```

This pipeline handles the **LIB-embedded format overlays** (`.BI`/`.CAM`/`.MC`/… DLLs). The
standalone companion game binaries (IP.EXE, WAIL32.DLL, msapi.dll, the comms DLLs) are not
part of it — their canonical home is the `fa-re` project via `import_targets.sh` (#252/#266).
The old `--secondary` / `secondary_projects/` pipeline was retired in #374.

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

Each is self-contained (`extends FAScript` for the shared helpers, its analysis
in `run()`). `run_all.sh` globs `Analyze*.java` and runs every one except the
overlay-only scripts (below), so this inventory *is* the roster — a new
`Analyze*.java` joins `run_all` automatically.

| Script | Subsystem | Output |
|---|---|---|
| `AnalyzeLAY.java` | Sky / atmosphere / horizon | `AnalyzeLAY.txt` |
| `AnalyzeHUD.java` | HUD draw, warning bits, bit 14 SP writer | `AnalyzeHUD.txt` |
| `AnalyzeDLG.java` | Dialog / UI system | `AnalyzeDLG.txt` |
| `AnalyzePROJ.java` | Projectile / missile physics | `AnalyzePROJ.txt` |
| `AnalyzeSEE.java` | Seeker / missile guidance | `AnalyzeSEE.txt` |
| `AnalyzeMM.java` | Mission map / campaign | `AnalyzeMM.txt` |
| `AnalyzeBI.java` | BI bytecode interpreter / AI, FRAME opcode | `AnalyzeBI.txt` |
| `AnalyzeBIFRAME.java` | BI FRAME opcode 0x28 consumer (CT state +0x7c/+0x7e) | `AnalyzeBIFRAME.txt` |
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
| `AnalyzeCB8.java` | CB8 / video image decoder | `AnalyzeCB8.txt` |
| `AnalyzeGameLoop.java` | Main loop / frame dispatch | `AnalyzeGameLoop.txt` |
| `AnalyzeRenderer.java` | Software renderer internals | `AnalyzeRenderer.txt` |
| `AnalyzePhysics.java` | Flight model / physics core | `AnalyzePhysics.txt` |
| `AnalyzeNetwork.java` | Multiplayer / netcode | `AnalyzeNetwork.txt` |
| `AnalyzeInput.java` | Input / joystick handling | `AnalyzeInput.txt` |
| `AnalyzeSHHeader.java` | SH shape header fields — radius/radius_world (#124) | `AnalyzeSHHeader.txt` |
| `AnalyzeSHDispatch.java` | SH interpreter vector_table; materializes handlers (#52) | `AnalyzeSHDispatch.txt` |
| `AnalyzeSHX86.java` | SH X86Unknown entry contract — do_start_asm/interp (#125) | `AnalyzeSHX86.txt` |
| `DumpGlobals.java` | Global variable inventory (+ named subset) | `DumpGlobals.csv`, `DumpGlobals_named.csv` |
| `RecoverStructs.java` | Struct field recovery | `RecoverStructs.txt` |
| `DumpAllFunctions.java` | Full function dump | `DumpAllFunctions.txt` |

### Utility scripts

| Script | Purpose | Headless? |
|---|---|---|
| `FAScript.java` | Base class — shared helpers | n/a |
| `ImportFASmsHeadless.java` | Import FA.SMS symbols (path from arg/env/default) | Yes |
| `DumpOverlayDLL.java` | Per-DLL dump for the format-overlay projects | Yes |
| `AnalyzeCAMDLL.java` / `AnalyzeMCDLL.java` | Deep dives on the CAM/MC overlay projects (run via `run_overlays.sh --analyze-cam`/`--analyze-mc`) | Yes |

### Launchers (Linux `.sh`)

| Launcher | Purpose |
|---|---|
| `_env.sh` | Shared env resolution, sourced by every `.sh` launcher |
| `run_ghidra.sh` | Run a single analysis script against FA.EXE |
| `run_all.sh` | Run all analysis scripts; `--setup` flag rebuilds the project first |
| `setup_project.sh` | One-shot: create project, import FA.EXE, load FA.SMS symbols |
| `extract_overlays.sh` | Unpack FA_1/FA_2.LIB and sort overlays by extension |
| `import_overlays.sh` | Patch PL→PE signature and import format-overlay DLLs into Ghidra |
| `run_overlays.sh` | Orchestrate overlay extract / import / analyze; see flags above |
| `import_targets.sh` | Import the #247 companion binaries (IP.EXE, WAIL32.DLL, comms DLLs, msapi.dll) into `fa-re` as their own programs |
| `apply_symbols.sh [BIN]` | Apply `db/symbols/` names to the project (per binary) |
| `apply_types.sh [BIN]` | Apply `db/types/` + the `type` column (per binary) |
| `export_inventory.sh [BIN]` | Export `db/inventory/<BIN>/` ground truth |
| `rebuild_audit.sh [BIN]` | Rebuild a binary from scratch and diff vs the local `db/inventory/` baseline |

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

That is the whole checklist: `run_all.sh` globs `Analyze*.java`, so a new FA.EXE
analyzer runs automatically — nothing to register. Add it to the `OVERLAY_ONLY`
exclusion in `run_all.sh` **only** if it targets an overlay project instead of
FA.EXE (like `AnalyzeCAMDLL`/`AnalyzeMCDLL`), and add a row to the inventory
table above.

## Superseded data directories

Three generations of companion-binary handling once coexisted; only the last is canonical
(the `fa-re` project via `import_targets.sh`, #252/#266). If your `$FA_PROJECT` predates #374
it may still hold superseded, regenerable data — safe to delete (~380 MB): `secondary_projects/`
and `overlay_projects/secondary` (the retired era-1/2 secondary pipelines), root-level
`fa_symbols.csv` and `analyze_secondary.log`, and `baseline-win/`. The `openfa/` checkout, if
present, is optional cross-reference reading — **not** a pipeline input.
