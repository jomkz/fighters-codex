import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import java.io.*;
import java.util.*;

public class DumpT2Loader extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\t2_loader.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // T2 terrain tile format. Remaining unknowns:
        // 1. Sub-header bytes 4–16 (class constants; 3 values by grid size — confirmed).
        //    Need world-space meaning.
        // 2. Surface class byte → PIC atlas tile mapping.
        // 3. Tile-summary record 0 algorithm (not NW corner, not dominant type).
        // Target: find T2 loader / terrain renderer in FA.EXE.

        // Search for T2-related symbols.
        out.println("// === FA.SMS symbols matching T2/terrain/tile/surface ===");
        for (Symbol sym : currentProgram.getSymbolTable().getAllSymbols(false)) {
            String name = sym.getName().toLowerCase();
            if (name.contains("terrain") || name.contains("tile") || name.contains("t2")
                    || name.contains("surface") || name.contains("tmap") || name.contains("tdic")) {
                out.println("// SYM: " + sym.getName() + " @ " + sym.getAddress());
            }
        }
        out.println();

        // Dump all functions whose names match terrain/tile patterns.
        out.println("// === Functions matching terrain/tile/T2 ===");
        Set<Long> dumpedAddrs = new LinkedHashSet<>();
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            String name = f.getName().toLowerCase();
            if (name.contains("terrain") || name.contains("tile") || name.contains("t2")
                    || name.contains("surface") || name.contains("tmap")) {
                long ep = f.getEntryPoint().getOffset();
                if (dumpedAddrs.add(ep)) {
                    out.println("// MATCH: " + f.getName() + " @ " + f.getEntryPoint());
                    dumpAt(ep);
                }
            }
        }

        // The T2 grid class bytes (4 constant values at sub-header+4..+16)
        // are read somewhere during map load or terrain query. Search for
        // references to the literal 0x14 (sub-header size) or the 3 class values.
        // Also search for ".T2" string reference to find the loader.
        out.println("// === Functions referencing .T2 filename string ===");
        for (Symbol sym : currentProgram.getSymbolTable().getAllSymbols(false)) {
            if (sym.getName().toUpperCase().endsWith(".T2")
                    || sym.getName().toUpperCase().contains("T2")) {
                out.println("// SYM: " + sym.getName() + " @ " + sym.getAddress());
                // Find functions that reference this symbol's address
                Address symAddr = sym.getAddress();
                for (Reference ref : currentProgram.getReferenceManager().getReferencesTo(symAddr)) {
                    Function f = currentProgram.getFunctionManager()
                        .getFunctionContaining(ref.getFromAddress());
                    if (f != null && dumpedAddrs.add(f.getEntryPoint().getOffset())) {
                        dumpAt(f.getEntryPoint().getOffset());
                    }
                }
            }
        }

        out.close();
        decompiler.dispose();
        println("Done. Output written to C:\\Temp\\t2_loader.txt");
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
