import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import java.io.*;

public class DumpHUDHVel extends GhidraScript {

    @Override
    public void run() throws Exception {
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\hud_hvel.txt");
        outFile.getParentFile().mkdirs();
        PrintWriter out = new PrintWriter(new FileWriter(outFile));

        // Find which functions reference the mystery struct fields at 0x265-0x26B
        // (globals DAT_005215c5 through DAT_005215cb)
        long[] probeAddrs = {
            0x005215c5L, 0x005215c7L, 0x005215c9L, 0x005215cbL
        };

        out.println("// === Cross-references to mystery HUD globals 0x5215c5-0x5215cb ===");
        java.util.Set<Long> callers = new java.util.LinkedHashSet<>();
        for (long probe : probeAddrs) {
            Address probeAddr = currentProgram.getAddressFactory()
                .getDefaultAddressSpace().getAddress(probe);
            out.println("// Refs to " + probeAddr + ":");
            currentProgram.getReferenceManager().getReferencesTo(probeAddr).forEach(ref -> {
                out.println("//   " + ref.getReferenceType() + " from " + ref.getFromAddress());
                Function f = currentProgram.getFunctionManager()
                    .getFunctionContaining(ref.getFromAddress());
                if (f != null) callers.add(f.getEntryPoint().getOffset());
            });
        }
        out.println();

        // Decompile each unique caller
        for (long addr : callers) {
            Address funcAddr = currentProgram.getAddressFactory()
                .getDefaultAddressSpace().getAddress(addr);
            Function func = getFunctionAt(funcAddr);
            if (func == null) func = currentProgram.getFunctionManager().getFunctionContaining(funcAddr);
            if (func != null) {
                DecompileResults results = decompiler.decompileFunction(func, 120, monitor);
                if (results.decompileCompleted()) {
                    out.println("// === " + func.getName() + " @ " + func.getEntryPoint() + " ===");
                    out.println(results.getDecompiledFunction().getC());
                    out.println();
                } else {
                    out.println("// === DECOMPILE FAILED: " + funcAddr + " ===\n");
                }
            }
        }

        // Also dump the full HUDDraw dispatcher (_HUDDraw@4) to see call order
        Address hudDrawAddr = currentProgram.getAddressFactory()
            .getDefaultAddressSpace().getAddress(0x00406040L);
        Function hudDraw = getFunctionAt(hudDrawAddr);
        if (hudDraw == null) hudDraw = currentProgram.getFunctionManager()
            .getFunctionContaining(hudDrawAddr);
        if (hudDraw != null) {
            out.println("// === HUDDraw dispatcher @ 00406040 ===");
            DecompileResults r = decompiler.decompileFunction(hudDraw, 120, monitor);
            if (r.decompileCompleted()) out.println(r.getDecompiledFunction().getC());
            out.println();
        }

        out.close();
        decompiler.dispose();
        println("Done. Output written to C:\\Temp\\hud_hvel.txt");
    }
}
