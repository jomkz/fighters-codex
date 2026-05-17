import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class DumpLAYFunctions2 extends GhidraScript {

    // Second pass: loader and gradient-reading functions
    static final long[] ADDRESSES = {
        // Functions between WRFogLayerUpdate and surrounding code
        0x004B2EA0L, // first function before WRGetLayer range
        0x004B2F00L,
        0x004B3210L, // just after WRGetLayer
        0x004B3300L,
        0x004B3400L,
        0x004B3500L,
        0x004B3600L,
        0x004B3700L,
        0x004B3800L,
        0x004B3900L,
        0x004B4000L,
        0x004B4100L,
        0x004B4200L,

        // Horizon sub-functions called by FUN_004c8fd4 and FUN_004c924c
        0x004C9066L, // called from Horizon2d
        0x004C90F4L, // called from Horizon2d
        0x004C91ADL, // called from Horizon2d
        0x004C9179L, // called from Horizon2d
        0x004C93C3L, // called from SolidHorizon
        0x004C93F6L, // called from SolidHorizon

        // FUN_004b48c0 — called from _T_DefaultHorizon with 0x583940 and auStack_1cc
        0x004B48C0L,

        // FUN_00447aa5 — called from _T_DefaultHorizon with aurora/cloud params
        0x00447AA5L,
    };

    @Override
    public void run() throws Exception {
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\lay_functions2.txt");
        outFile.getParentFile().mkdirs();
        PrintWriter out = new PrintWriter(new FileWriter(outFile));

        // Print all symbols in the 0x4B2000–0x4B5000 range for orientation
        out.println("// === SYMBOLS in 0x4B2000-0x4B5000 ===");
        SymbolTable symTable = currentProgram.getSymbolTable();
        Address lo = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(0x4B2000L);
        Address hi = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(0x4B5000L);
        SymbolIterator it = symTable.getPrimarySymbolIterator(lo, true);
        while (it.hasNext()) {
            Symbol sym = it.next();
            if (sym.getAddress().compareTo(hi) > 0) break;
            out.println("//   " + sym.getName() + " @ " + sym.getAddress());
        }
        out.println();

        // Also show what writes to DAT_00583da8 (LAYER struct pointer)
        out.println("// === References to DAT_00583da8 (LAYER struct ptr) ===");
        Address layerPtr = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(0x00583da8L);
        it = symTable.getPrimarySymbolIterator(layerPtr, true);
        while (it.hasNext()) {
            Symbol sym = it.next();
            if (!sym.getAddress().equals(layerPtr)) break;
            out.println("//   " + sym.getName() + " @ " + sym.getAddress());
        }
        out.println();

        Set<Long> done = new HashSet<>();
        for (long addr : ADDRESSES) {
            if (done.contains(addr)) continue;
            done.add(addr);
            Address funcAddr = currentProgram.getAddressFactory()
                .getDefaultAddressSpace().getAddress(addr);
            dumpAt(funcAddr, decompiler, out);
        }

        out.close();
        decompiler.dispose();
        println("Done. Output written to C:\\Temp\\lay_functions2.txt");
    }

    private void dumpAt(Address funcAddr, DecompInterface decompiler, PrintWriter out) throws Exception {
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
