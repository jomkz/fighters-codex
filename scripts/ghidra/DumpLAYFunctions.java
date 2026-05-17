import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import java.io.*;
import java.util.*;

public class DumpLAYFunctions extends GhidraScript {

    static final String[] SYMBOL_NAMES = {};

    static final long[] ADDRESSES = {
        // Layer file loading — reads LAYER_FILE_HEADER, pointer table, gradient sub-blocks
        0x004B3190L, // _WRGetLayer@8          — main layer loader
        0x004B4320L, // _WRFogLayerUpdate      — per-frame layer update

        // Horizon / gradient rendering — reads colour sub-blocks via LAYER pointers
        0x004AACA0L, // _T_InitHorizonProc@0   — horizon init; sets gradient pointers
        0x004AACE0L, // _T_HorizonProc         — horizon render entry (imported by LAY DLLs)
        0x004AACF0L, // _T_DefaultHorizon      — default horizon path
        0x004C8FD4L, // Horizon2d              — 2D horizon renderer
        0x004C924CL, // _SolidHorizon          — solid-colour horizon
        0x004C942CL, // _GouraudHorizon        — Gouraud-shaded; reads colour remap tables
    };

    @Override
    public void run() throws Exception {
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\lay_functions.txt");
        outFile.getParentFile().mkdirs();
        PrintWriter out = new PrintWriter(new FileWriter(outFile));

        Set<Long> done = new HashSet<>();

        // Search all symbols for LAY/cloud/layer/horizon-related names
        SymbolTable symTable = currentProgram.getSymbolTable();
        SymbolIterator allSyms = symTable.getAllSymbols(false);
        while (allSyms.hasNext()) {
            Symbol sym = allSyms.next();
            String name = sym.getName().toLowerCase();
            if (name.contains("cloud") || name.contains("layer") || name.contains("horizon") ||
                name.contains("t_cloud") || name.contains("hdrptr") || name.contains("curlayer") ||
                name.contains("nextlayer") || name.contains("gradient") || name.contains("sky") ||
                name.contains("atmos")) {
                out.println("// SYMBOL: " + sym.getName() + " @ " + sym.getAddress() + " [" + sym.getSymbolType() + "]");
            }
        }
        out.println();

        // Dump by symbol name
        for (String symName : SYMBOL_NAMES) {
            SymbolIterator it = symTable.getSymbols(symName);
            boolean found = false;
            while (it.hasNext()) {
                Symbol sym = it.next();
                Address addr = sym.getAddress();
                if (done.contains(addr.getOffset())) continue;
                done.add(addr.getOffset());
                found = true;
                dumpAt(addr, decompiler, out);
            }
            if (!found) {
                out.println("// === SYMBOL NOT FOUND: " + symName + " ===\n");
            }
        }

        // Dump by address
        for (long addr : ADDRESSES) {
            if (done.contains(addr)) continue;
            done.add(addr);
            Address funcAddr = currentProgram.getAddressFactory()
                .getDefaultAddressSpace().getAddress(addr);
            dumpAt(funcAddr, decompiler, out);
        }

        out.close();
        decompiler.dispose();
        println("Done. Output written to C:\\Temp\\lay_functions.txt");
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
