import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class DumpMMLayerSlot3 extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;
    private Set<Long> dumped = new LinkedHashSet<>();

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\mm_layer_slot3.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // Context:
        //   FUN_0044f180 and FUN_00430a90 are state serializers that always write slot 0.
        //   The actual runtime MM text parser is elsewhere.
        //   The plain "layer" string at 0x4f80f8 is referenced by:
        //     FUN_0047a130 (insn 0x47a462) — likely the MM text keyword parser
        //     FUN_00481c10 (insns 0x481df8, 0x481df1) — possible callback reader
        //     FUN_004ad3c0 (insn 0x4ad883) — file loader
        //
        // Also: need to check if FUN_004ad3c0 is a generic keyword-value parser,
        // and whether it passes the slot index anywhere.

        out.println("// === FUN_0047a130 (refs plain 'layer' string @ 0x4f80f8) ===");
        dumpAt(0x0047a130L);

        out.println("\n// === FUN_00481c10 (refs plain 'layer' string) ===");
        dumpAt(0x00481c10L);

        out.println("\n// === Callers of FUN_0047a130 ===");
        dumpAllCallers(toAddr(0x0047a130L), "FUN_0047a130");

        out.close();
        decompiler.dispose();
        println("Output: C:\\Temp\\mm_layer_slot3.txt");
    }

    private void dumpAllCallers(Address targetAddr, String label) throws Exception {
        ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(targetAddr);
        Set<Long> callersSeen = new LinkedHashSet<>();
        while (refs.hasNext()) {
            Reference ref = refs.next();
            if (!ref.getReferenceType().isCall()) continue;
            Function caller = currentProgram.getFunctionManager()
                .getFunctionContaining(ref.getFromAddress());
            if (caller == null) continue;
            long callerVA = caller.getEntryPoint().getOffset();
            if (callersSeen.contains(callerVA)) continue;
            callersSeen.add(callerVA);
            out.println("\n// Caller of " + label + ": "
                + caller.getName() + " @ 0x" + Long.toHexString(callerVA));
            dumpAt(callerVA);
        }
        if (callersSeen.isEmpty()) {
            out.println("// No call-xref callers found for " + label);
        }
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
