# scripts/

Reverse-engineering helper scripts. Not part of the toolkit build.

## ghidra/ImportFASms.java

Ghidra Java script that bulk-imports all 3,829 FA.SMS symbols into the current program as named labels.

**Setup:**

1. In Ghidra, open **Tools → Script Manager**.
2. Click the script-directory icon and add the `scripts/ghidra/` directory from this repo.
3. `ImportFASms` will appear under the **FightersAnthology** category.

**Usage:**

1. Open FA.EXE (or an overlay DLL rebased to `0x00400000`) in Ghidra and let auto-analysis finish.
2. Run `ImportFASms`. A file dialog will prompt for `FA.SMS`.
3. The script creates a named label at each symbol's virtual address. Progress is shown in the status bar.

**Overlay DLLs:**

The FA.SMS VAs are relative to FA.EXE's image base (`0x00400000`). Overlay DLLs (`.HUD`, `.DLG`, `.FNT`, etc.) have their own preferred base. To use the symbol names as cross-references when tracing overlay DLL imports:

- Open the overlay DLL in a separate Ghidra project.
- Optionally rebase the DLL to `0x00400000` (Edit → Memory Map → Set Image Base) and run `ImportFASms` to label the imported engine functions it calls.
