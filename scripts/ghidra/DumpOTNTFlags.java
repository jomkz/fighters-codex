import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import java.io.*;
import java.util.*;

public class DumpOTNTFlags extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\otnt_flags.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // OT/NT ot_flags bit semantics.
        // Confirmed bits: 0, 1, 2, 3, 4, 6, 7, 12, 13, 14, 16, 17, 24, 27.
        // Unconfirmed (need Ghidra): bits 5, 8, 9, 10, 11, 15, 18, 19, 20, 22, 25, 26.
        // Target: trace functions that read ot_flags via known masks.

        // _GVProc / ship fire-control dispatcher — reads NT hardpoint bit 1 ($2).
        // Also confirms NT hardpoint semantics for IOWA/KIROV/SSN9.
        out.println("// === _GVProc (ship fire-control / vehicle AI dispatcher) ===");
        for (Symbol sym : currentProgram.getSymbolTable().getSymbols("_GVProc")) {
            dumpAt(sym.getAddress().getOffset());
        }

        // Dump all functions whose names suggest OT/NT/vehicle flag processing.
        out.println("// === FA.SMS symbols matching OT/NT/vehicle/flag/capability ===");
        for (Symbol sym : currentProgram.getSymbolTable().getAllSymbols(false)) {
            String name = sym.getName().toLowerCase();
            if (name.contains("ot_flag") || name.contains("otflag")
                    || name.contains("capability") || name.contains("gvproc")
                    || name.contains("vehicle") || name.contains("naval")) {
                out.println("// SYM: " + sym.getName() + " @ " + sym.getAddress());
            }
        }
        out.println();

        // Functions that reference ot_flags — search by reading references to
        // the capability dword offset in the OBJ_TYPE struct.
        // OBJ_TYPE+0x0C is the capability flags dword (confirmed from BRF analysis).
        // The engine reads it from runtime objects at various offsets.
        // Also dump the object-creation / spawn function which copies OBJ_TYPE fields.
        out.println("// === Functions matching object/spawn/OT/NT patterns ===");
        Set<Long> dumpedAddrs = new LinkedHashSet<>();
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            String name = f.getName().toLowerCase();
            if (name.contains("spawn") || name.contains("create") && name.contains("obj")
                    || name.contains("_gv") || name.contains("naval")
                    || name.contains("hardpoint") || name.contains("hardpt")) {
                long ep = f.getEntryPoint().getOffset();
                if (dumpedAddrs.add(ep)) {
                    out.println("// MATCH: " + f.getName() + " @ " + f.getEntryPoint());
                    dumpAt(ep);
                }
            }
        }

        // NT hardpoint bit 1 ($2) — trace ship fire-control.
        // Known ships: IOWA has $a HPs (PHALANX/SEA_SPAR), KIROV $a (AAA30),
        // SSN9 uses $8. The $2 bit is not surface-strike-missile.
        // Dump any NT hardpoint evaluation function.
        out.println("// === @HARDFire (if present) or hardpoint fire dispatcher ===");
        for (Symbol sym : currentProgram.getSymbolTable().getAllSymbols(false)) {
            String name = sym.getName().toLowerCase();
            if (name.contains("hard") && (name.contains("fire") || name.contains("arm")
                    || name.contains("select") || name.contains("proc"))) {
                Function f = currentProgram.getFunctionManager()
                    .getFunctionAt(sym.getAddress());
                if (f != null && dumpedAddrs.add(f.getEntryPoint().getOffset())) {
                    out.println("// MATCH: " + sym.getName() + " @ " + sym.getAddress());
                    dumpAt(f.getEntryPoint().getOffset());
                }
            }
        }

        out.close();
        decompiler.dispose();
        println("Done. Output written to C:\\Temp\\otnt_flags.txt");
    }

    private void dumpAt(long address) throws Exception {
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
