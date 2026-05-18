import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class DumpPROJPhysicsInit extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;
    private Set<Long> dumped = new LinkedHashSet<>();

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\proj_physics_init.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // Goal: identify PROJ_TYPE fields at offsets 0x50-0x6E (30 bytes unresolved).
        // _PROJInit@0 and _PROJAdd@40 are the best candidates — they initialize live
        // projectile state from PROJ_TYPE, so will access the physics fields directly.
        //
        // Also dump _PROJEngineState@0 (thrust), ?PROJMoveProc (movement per tick),
        // and _PROJFire@16 (launch) as supplementary sources.

        out.println("// === _PROJInit@0 (0x4c06a0) ===");
        dumpAt(0x004c06a0L);

        out.println("\n// === _PROJAdd@40 (0x4c0a90) ===");
        dumpAt(0x004c0a90L);

        out.println("\n// === ?PROJMoveProc@@YAXD@Z (0x4c11b0) ===");
        dumpAt(0x004c11b0L);

        out.println("\n// === _PROJEngineState@0 (0x4c1170) ===");
        dumpAt(0x004c1170L);

        out.println("\n// === _PROJFire@16 (0x4c2170) ===");
        dumpAt(0x004c2170L);

        out.println("\n// === _PROJLockUpdate@0 (0x4c0960) ===");
        dumpAt(0x004c0960L);

        out.println("\n// === _PROJHitChance@28 (0x4c3380) ===");
        dumpAt(0x004c3380L);

        // Also dump the function that contains _PROJProc address (FUN_004c1f10).
        out.println("\n// === FUN_004c1f10 (contains _PROJProc addr 0x4c1f50) ===");
        dumpAt(0x004c1f10L);

        // Dump surrounding functions in 0x4c0000-0x4c2800 to catch any physics helpers.
        out.println("\n// === Functions 0x4c0500-0x4c1200 (PROJ physics cluster) ===");
        for (Function fn : currentProgram.getFunctionManager().getFunctions(true)) {
            long ep = fn.getEntryPoint().getOffset();
            if (ep >= 0x004c0500L && ep <= 0x004c1200L && !dumped.contains(ep)) {
                dumpAt(ep);
            }
        }

        out.close();
        decompiler.dispose();
        println("Output: C:\\Temp\\proj_physics_init.txt");
    }

    private void dumpAt(long va) throws Exception {
        if (dumped.contains(va)) return;
        dumped.add(va);
        Address addr = toAddr(va);
        Function fn = currentProgram.getFunctionManager().getFunctionAt(addr);
        if (fn == null) fn = currentProgram.getFunctionManager().getFunctionContaining(addr);
        if (fn == null) {
            out.println("// NOT FOUND at 0x" + Long.toHexString(va));
            return;
        }
        out.println("// --- " + fn.getName() + " @ 0x" + Long.toHexString(va) + " ---");
        DecompileResults res = decompiler.decompileFunction(fn, 120, monitor);
        if (res != null && res.getDecompiledFunction() != null) {
            out.println(res.getDecompiledFunction().getC());
        } else {
            out.println("// decompile failed: " + fn.getName());
        }
        out.println();
    }
}
