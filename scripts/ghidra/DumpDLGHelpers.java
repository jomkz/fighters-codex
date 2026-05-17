import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import java.io.*;
import java.util.*;

public class DumpDLGHelpers extends GhidraScript {

    @Override
    public void run() throws Exception {
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\dlg_helpers.txt");
        outFile.getParentFile().mkdirs();
        PrintWriter out = new PrintWriter(new FileWriter(outFile));

        // _ChoosePreload is dispatched via pointer — no direct callers.
        // Decode record param semantics by decompiling:
        //   1. func_0x00489840 — state/stack helper called from _ChoosePreload
        //   2. FUN_0040d5f0 — preload loader called first in _ChoosePreload
        //   3. Follow DATA refs to 0x004897f0 to find the dispatch table + dispatcher

        out.println("// === func_0x00489840 (state helper used by _ChoosePreload) ===");
        dumpAt(0x00489840L, decompiler, out);

        out.println("// === FUN_0040d5f0 (first call in _ChoosePreload) ===");
        dumpAt(0x0040d5f0L, decompiler, out);

        // Find all references to _ChoosePreload (0x004897f0) — expect DATA refs
        // from a function-pointer table
        out.println("// === References to _ChoosePreload (0x004897f0) ===");
        Address cpAddr = currentProgram.getAddressFactory()
            .getDefaultAddressSpace().getAddress(0x004897f0L);
        Set<Long> tableCallers = new LinkedHashSet<>();
        currentProgram.getReferenceManager().getReferencesTo(cpAddr).forEach(ref -> {
            out.println("//   " + ref.getReferenceType()
                + " from " + ref.getFromAddress());
            Function f = currentProgram.getFunctionManager()
                .getFunctionContaining(ref.getFromAddress());
            if (f != null) {
                out.println("//     in function: " + f.getName() + " @ " + f.getEntryPoint());
                tableCallers.add(f.getEntryPoint().getOffset());
            }
        });
        out.println();
        for (long addr : tableCallers) {
            dumpAt(addr, decompiler, out);
        }

        // Also decompile blink/context functions that likely read 4-word params
        // FUN_0048b2e0 is called from FUN_004897c0 (same pattern as _ChoosePreload)
        out.println("// === func_0x0048b2e0 (called from FUN_004897c0, sibling of _ChoosePreload) ===");
        dumpAt(0x0048b2e0L, decompiler, out);

        // Enumerate callers of func_0x00489840 (names only — do NOT decompile all, too many)
        out.println("// === Callers of func_0x00489840 (list only) ===");
        currentProgram.getReferenceManager().getReferencesTo(
            currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(0x00489840L)
        ).forEach(ref -> {
            if (ref.getReferenceType().isCall()) {
                Function f = currentProgram.getFunctionManager()
                    .getFunctionContaining(ref.getFromAddress());
                if (f != null) out.println("//   " + f.getName() + " @ " + f.getEntryPoint());
            }
        });
        out.println();

        out.close();
        decompiler.dispose();
        println("Done. Output written to C:\\Temp\\dlg_helpers.txt");
    }

    private void dumpAt(long address, DecompInterface decompiler, PrintWriter out) throws Exception {
        Address funcAddr = currentProgram.getAddressFactory()
            .getDefaultAddressSpace().getAddress(address);
        Function func = getFunctionAt(funcAddr);
        if (func == null) func = currentProgram.getFunctionManager().getFunctionContaining(funcAddr);
        if (func == null) {
            DisassembleCommand disCmd = new DisassembleCommand(funcAddr, null, true);
            disCmd.applyTo(currentProgram, monitor);
            CreateFunctionCmd createCmd = new CreateFunctionCmd(funcAddr);
            createCmd.applyTo(currentProgram, monitor);
            func = getFunctionAt(funcAddr);
        }
        if (func != null) {
            DecompileResults results = decompiler.decompileFunction(func, 120, monitor);
            if (results.decompileCompleted()) {
                out.println("// === " + func.getName() + " @ " + func.getEntryPoint() + " ===");
                out.println(results.getDecompiledFunction().getC());
                out.println();
            } else {
                out.println("// === DECOMPILE FAILED: " + funcAddr + " ===\n");
            }
        } else {
            out.println("// === NO FUNCTION AT: " + funcAddr + " ===\n");
        }
    }
}
