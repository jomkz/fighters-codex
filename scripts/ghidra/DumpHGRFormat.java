import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class DumpHGRFormat extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;
    private Set<Long> dumped = new LinkedHashSet<>();

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\hgr_format.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // FUN_00480150 calls FUN_004543c0(0, &DAT_004fb1e8, '\0', '\x01') to load HGR.
        // DAT_004fb198 selects carrier vs land-base hangar.
        // FUN_004809d0 initializes ?hangarName@@3PADA from DAT_004fbbf0.
        //
        // Goal: find the HGR record layout (aircraft slots, icon positions, camera angle).
        // Dump FUN_004543c0 (HGR file loader) and trace what it reads from the DLL.

        out.println("// === FUN_004543c0 (HGR file loader) ===");
        dumpAt(0x004543c0L);

        // FUN_004809d0: hangar name init (from previous output) — for reference.
        out.println("\n// === FUN_004809d0 (hangar name init) ===");
        dumpAt(0x004809d0L);

        // FUN_00480150: HGR load trigger (calls FUN_004543c0).
        out.println("\n// === FUN_00480150 (HGR load trigger) ===");
        dumpAt(0x00480150L);

        // FUN_004801a0: HGR unload (DAT_004fb198 guard).
        out.println("\n// === FUN_004801a0 (HGR unload) ===");
        dumpAt(0x004801a0L);

        // Dump all callers of FUN_004543c0 to find all HGR-file consumers.
        out.println("\n// === Callers of FUN_004543c0 (HGR loader) ===");
        Address loaderAddr = toAddr(0x004543c0L);
        Set<Long> callersSeen = new LinkedHashSet<>();
        for (Reference ref : currentProgram.getReferenceManager().getReferencesTo(loaderAddr)) {
            if (ref.getReferenceType().isCall()) {
                Function fn = currentProgram.getFunctionManager()
                        .getFunctionContaining(ref.getFromAddress());
                if (fn != null && callersSeen.add(fn.getEntryPoint().getOffset())) {
                    out.println("// CALLER: " + fn.getName() + " @ " + fn.getEntryPoint());
                }
            }
        }
        for (long va : callersSeen) {
            if (!dumped.contains(va)) dumpAt(va);
        }

        // Also dump functions near FUN_00480150 (0x480000-0x480600) —
        // the hangar initialization cluster.
        out.println("\n// === Functions near HGR init (0x480000-0x480600) ===");
        for (Function fn : currentProgram.getFunctionManager().getFunctions(true)) {
            long ep = fn.getEntryPoint().getOffset();
            if (ep >= 0x00480000L && ep <= 0x00480600L && !dumped.contains(ep)) {
                dumpAt(ep);
            }
        }

        // Read DAT_004fbbf0 — the source hangar name string.
        // Find all references to it to understand how it's set (which H_AIRB variant).
        out.println("\n// === References to DAT_004fbbf0 (hangar name source) ===");
        Address hangarSrc = toAddr(0x004fbbf0L);
        for (Reference ref : currentProgram.getReferenceManager().getReferencesTo(hangarSrc)) {
            Function fn = currentProgram.getFunctionManager()
                    .getFunctionContaining(ref.getFromAddress());
            if (fn != null && !dumped.contains(fn.getEntryPoint().getOffset())) {
                out.println("// ref at " + ref.getFromAddress() + " from " + fn.getName());
                dumpAt(fn.getEntryPoint().getOffset());
            }
        }

        out.close();
        decompiler.dispose();
        println("Output: C:\\Temp\\hgr_format.txt");
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
