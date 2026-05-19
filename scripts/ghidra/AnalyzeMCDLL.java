// Mission condition DLL logic analysis.
// Runs against %FA_PROJECT%\overlay_projects\mc\
// For each MC DLL: dumps the exported entry point, traces all condition-check
// paths, and identifies: which object aliases are tested, which conditions
// trigger mission success or failure, and what time thresholds are used.
// Also traces FUN_00495e80 to clarify its role in the dispatch chain.
//
// Invoke via run_overlays.bat --analyze MC
// Output: %FA_PROJECT%\output\AnalyzeMCDLL.txt

import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.util.*;

public class AnalyzeMCDLL extends FAScript {

    @Override
    public void run() throws Exception {
        openOutputAppend("AnalyzeMCDLL");

        // -----------------------------------------------------------------------
        // Exported entry point (condition evaluator)
        // -----------------------------------------------------------------------
        header("MC -- exported entry point(s)");
        SymbolTable st = currentProgram.getSymbolTable();
        FunctionManager fm = currentProgram.getFunctionManager();
        List<Long> exportVas = new ArrayList<>();
        for (Symbol sym : st.getAllSymbols(false)) {
            if (sym.isExternalEntryPoint()) {
                Function fn = fm.getFunctionAt(sym.getAddress());
                if (fn != null) {
                    out.println("// EXPORT: " + sym.getName() + " @ " + sym.getAddress());
                    exportVas.add(fn.getEntryPoint().getOffset());
                    dumpAt(fn.getEntryPoint().getOffset());
                }
            }
        }

        // -----------------------------------------------------------------------
        // Condition-check call trace from each export
        // -----------------------------------------------------------------------
        header("MC -- condition-check paths (callees of exported entry)");
        for (long exportVa : exportVas) {
            Function exportFn = fm.getFunctionAt(toAddr(exportVa));
            if (exportFn == null) continue;
            out.println("// Callees of " + exportFn.getName() + ":");
            for (Function callee : exportFn.getCalledFunctions(monitor)) {
                out.println("//   " + callee.getName() + " @ " + callee.getEntryPoint());
                dumpAt(callee.getEntryPoint().getOffset());
            }
        }

        // -----------------------------------------------------------------------
        // Dispatch chain function (from FA.EXE -- may appear as import thunk)
        // -----------------------------------------------------------------------
        header("MC -- dispatch chain FUN_00495e80 (if present as thunk)");
        long dispatchVa = 0x00495e80L;
        Function dispatch = fm.getFunctionAt(toAddr(dispatchVa));
        if (dispatch != null) {
            dumpAt(dispatchVa);
        } else {
            out.println("// FUN_00495e80 not present in this overlay (it lives in FA.EXE)");
            out.println("// Look for it as an import reference / external call in the export body.");
        }

        // -----------------------------------------------------------------------
        // Scan for known mission-outcome call patterns
        // -----------------------------------------------------------------------
        header("MC -- mission outcome markers (string scan)");
        searchStrings(new String[]{
            "SUCCESS", "FAILED", "Mission", "success", "failed",
            "MISSIONSuccess", "campaignFailed", "OBJAlias",
            "_currentTime", "currentTime", "time", "Time"
        });

        // -----------------------------------------------------------------------
        // All remaining functions (VA order)
        // -----------------------------------------------------------------------
        header("MC -- all functions (VA order)");
        List<Function> allFns = new ArrayList<>();
        for (Function fn : fm.getFunctions(true)) allFns.add(fn);
        allFns.sort((a, b) -> Long.compare(
                a.getEntryPoint().getOffset(), b.getEntryPoint().getOffset()));
        for (Function fn : allFns) {
            dumpAt(fn.getEntryPoint().getOffset());
        }

        out.println("\n// Total functions dumped: " + dumped.size());
        closeOutput();
    }
}
