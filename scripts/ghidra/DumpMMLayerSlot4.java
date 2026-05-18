import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class DumpMMLayerSlot4 extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;
    private Set<Long> dumped = new LinkedHashSet<>();

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\mm_layer_slot4.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // Goal: trace what FUN_0047a510 does with the "layer <name> <slot>" MM line.
        // FUN_0047a130 calls: FUN_0047a510(0x4f80f8 = "layer", param_1 = full MM line)
        // FUN_0047a510 is the keyword dispatch handler for all MM line types.

        out.println("// === FUN_0047a510 (MM keyword handler) ===");
        dumpAt(0x0047a510L);

        // Also dump the keyword table entries near 0x4f80f8 to understand dispatch
        // and dump any function that FUN_0047a510 calls for the "layer" keyword.

        out.close();
        decompiler.dispose();
        println("Output: C:\\Temp\\mm_layer_slot4.txt");
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
