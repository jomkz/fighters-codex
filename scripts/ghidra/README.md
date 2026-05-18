# Ghidra Scripts — FA.EXE Reverse Engineering

Scripts for decompiling and analysing Jane's Fighters Anthology (`FA.EXE`) using Ghidra.

## Prerequisites

| Tool | Version | Default path |
|---|---|---|
| [Ghidra](https://ghidra-sre.org/) | 12.1 | `C:\tools\ghidra_12.1_PUBLIC` |
| JDK | 21+ (JDK 26 tested) | `C:\java\jdk-26.0.1` |
| FA.EXE | any FA install | — |
| FA.SMS | same FA install | — |

Edit the variables at the top of `run_ghidra.bat` if your paths differ:

```bat
set JAVA_HOME=C:\java\jdk-26.0.1
set GHIDRA_HOME=C:\tools\ghidra_12.1_PUBLIC
set FA_PROJECT=%USERPROFILE%\src\fa
```

---

## First-time setup

### 1. Create the Ghidra project

1. Launch Ghidra and choose **File → New Project**
2. Create a **Non-Shared Project** at `%USERPROFILE%\src\fa`, name it `fa-re`
3. **File → Import File** → select `FA.EXE`
4. Accept the default PE import options and click **OK**
5. When prompted to analyse, click **Yes** and accept the default analysers

Auto-analysis takes a few minutes. Wait for it to finish before proceeding.

### 2. Import FA.SMS symbols

FA.SMS is the debug symbol table shipped with the game. Importing it names ~4 000 functions and globals, which makes the decompiler output far more readable.

1. In the Ghidra CodeBrowser, open `FA.EXE`
2. **Window → Script Manager**, locate `ImportFASms.java` and run it
3. When prompted, select `FA.SMS` from your FA install directory

> **Note:** `ImportFASms.java` must be run from the Ghidra GUI — it uses an interactive file picker and cannot run headless.

### 3. Verify the project location

The headless scripts expect the project at:

```
%USERPROFILE%\src\fa\fa-re.gpr
%USERPROFILE%\src\fa\fa-re.rep\
```

If you placed the project elsewhere, update `FA_PROJECT` in `run_ghidra.bat`.

---

## Running scripts

**All subsystems (single output file):**

```bat
scripts\ghidra\run_all.bat
```

Output: `%FA_PROJECT%\output\AnalyzeFA.txt`

**Single subsystem:**

```bat
scripts\ghidra\run_ghidra.bat AnalyzeLAY.java
```

Output: `%FA_PROJECT%\output\AnalyzeLAY.txt`

---

## Script inventory

| Script | Subsystem | Output |
|---|---|---|
| `AnalyzeFA.java` | Master — runs all subsystems | `AnalyzeFA.txt` |
| `AnalyzeLAY.java` | Sky / atmosphere / horizon | `AnalyzeLAY.txt` |
| `AnalyzeHUD.java` | HUD draw, warning bits | `AnalyzeHUD.txt` |
| `AnalyzeDLG.java` | Dialog / UI system | `AnalyzeDLG.txt` |
| `AnalyzePROJ.java` | Projectile / missile physics | `AnalyzePROJ.txt` |
| `AnalyzeSEE.java` | Seeker / missile guidance | `AnalyzeSEE.txt` |
| `AnalyzeMM.java` | Mission map / campaign | `AnalyzeMM.txt` |
| `AnalyzeBI.java` | BI bytecode interpreter / AI | `AnalyzeBI.txt` |
| `AnalyzeECM.java` | ECM / jammer | `AnalyzeECM.txt` |
| `AnalyzeHGR.java` | Hangar / airbase rendering | `AnalyzeHGR.txt` |
| `AnalyzeMUS.java` | Music / SEQ | `AnalyzeMUS.txt` |
| `AnalyzeOTNT.java` | Vehicle OT/NT classification | `AnalyzeOTNT.txt` |
| `AnalyzeT2.java` | Terrain tile system | `AnalyzeT2.txt` |
| `AnalyzeGAS.java` | Fuel, hardpoints, BRF | `AnalyzeGAS.txt` |
| `FAScript.java` | Base class (not run directly) | — |
| `ImportFASms.java` | Import FA.SMS symbols (GUI only) | — |

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
