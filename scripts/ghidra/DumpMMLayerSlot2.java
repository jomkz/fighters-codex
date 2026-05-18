import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class DumpMMLayerSlot2 extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;
    private Set<Long> dumped = new LinkedHashSet<>();

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\mm_layer_slot2.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // Goal: resolve MM `layer <name>.LAY <index>` slot index semantics.
        //
        // From DumpMMLayerSlot:
        //   - FUN_004b3480 is NOT the slot consumer: it takes an altitude, not a slot.
        //   - GetLayerByIndex (FUN_004b3170) stores `param_1` in DAT_0050c8b4 and sets
        //     DAT_00583da8 = (&DAT_00580db0)[param_1]. Its only known caller uses indices 0 and 3.
        //   - "layer %s 0" at 0x4f375c is referenced from FUN_0044f180 (7×) and FUN_00430a90 (7×).
        //     These are likely the MM writer and MM parser respectively.
        //
        // Plan:
        //   1. Dump FUN_0044f180 (MM writer candidate — 7 xrefs to "layer %s 0").
        //   2. Dump FUN_00430a90 (MM reader candidate — 7 xrefs to same string).
        //   3. Dump the GetLayerByIndex caller FUN_0043a5c0 around the GetLayerByIndex calls
        //      to find which slot value is passed and under what conditions.
        //   4. Dump FUN_0043a5c0 fully (where GetLayerByIndex(3) and (0) are called).

        out.println("// === FUN_0044f180 (MM writer candidate, refs 'layer %s 0') ===");
        dumpAt(0x0044f180L);

        out.println("\n// === FUN_00430a90 (MM reader candidate, refs 'layer %s 0') ===");
        dumpAt(0x00430a90L);

        out.println("\n// === FUN_0043a5c0 (GetLayerByIndex caller) ===");
        dumpAt(0x0043a5c0L);

        out.close();
        decompiler.dispose();
        println("Output: C:\\Temp\\mm_layer_slot2.txt");
    }

    private void dumpAt(long va) throws Exception {
        if (dumped.contains(va)) {
            out.println("// (already dumped above)");
            return;
        }
        dumped.add(va);
        Address addr = currentProgram.getAddressFactory()
            .getDefaultAddressSpace().getAddress(va);
        Function fn = currentProgram.getFunctionManager().getFunctionAt(addr);
        if (fn == null) {
            out.println("// NOT FOUND at 0x" + Long.toHexString(va));
            return;
        }
        out.println("// --- " + fn.getName() + " @ 0x" + Long.toHexString(va) + " ---");
        DecompileResults res = decompiler.decompileFunction(fn, 120, monitor);
        if (res != null && res.getDecompiledFunction() != null) {
            out.println(res.getDecompiledFunction().getC());
        } else {
            out.println("// decompile failed");
        }
    }
}
