import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import java.io.*;
import java.util.*;

public class DumpDLGChoosePreload extends GhidraScript {

    @Override
    public void run() throws Exception {
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\dlg_choosepreload.txt");
        outFile.getParentFile().mkdirs();
        PrintWriter out = new PrintWriter(new FileWriter(outFile));

        // _ChoosePreload is FUN_004897f0; it only calls two helpers.
        // The (379, 80, 238, 361) param encoding is in its callers.
        long choosePreloadAddr = 0x004897F0L;

        out.println("// === _ChoosePreload (FUN_004897f0) ===");
        dumpAt(choosePreloadAddr, decompiler, out);

        // Find all callers of _ChoosePreload
        out.println("// === Callers of _ChoosePreload ===");
        Address cpAddr = currentProgram.getAddressFactory()
            .getDefaultAddressSpace().getAddress(choosePreloadAddr);
        Set<Long> callers = new LinkedHashSet<>();
        currentProgram.getReferenceManager().getReferencesTo(cpAddr).forEach(ref -> {
            if (ref.getReferenceType().isCall()) {
                Function f = currentProgram.getFunctionManager()
                    .getFunctionContaining(ref.getFromAddress());
                if (f != null) {
                    out.println("//   CALL from " + ref.getFromAddress() + " in " + f.getName());
                    callers.add(f.getEntryPoint().getOffset());
                }
            }
        });
        out.println();

        for (long addr : callers) {
            dumpAt(addr, decompiler, out);
        }

        // Also dump the two helpers called by _ChoosePreload for context
        // Find them by decompiling _ChoosePreload and looking at its callees
        out.println("// === Additional context: DLG dialog-type dispatch near _ChoosePreload ===");
        // Dialog init function that sets up bounding boxes
        long[] contextAddrs = {
            0x00489830L, // helper 1 immediately after _ChoosePreload
            0x00489870L, // helper 2
            0x004897C0L, // function before _ChoosePreload in the dispatch block
        };
        for (long addr : contextAddrs) {
            dumpAt(addr, decompiler, out);
        }

        out.close();
        decompiler.dispose();
        println("Done. Output written to C:\\Temp\\dlg_choosepreload.txt");
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
