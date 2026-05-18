import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.mem.*;
import java.io.*;
import java.util.*;

public class DumpMMLayerSlot extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;
    private Set<Long> dumped = new LinkedHashSet<>();

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\mm_layer_slot.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // Goal: resolve the MM `layer <name>.LAY <index>` slot index semantics.
        //
        // Known facts:
        //   - ParseLayerFile (FUN_004b4370) takes only one param (filename).
        //   - Both found callers hardcode "day1.LAY" — not the MM mission loader.
        //   - FUN_004b3480 is called right after ParseLayerFile in both known callers,
        //     always with arg 0. It may be the function that uses the slot index.
        //   - MM file slot indices observed: 0 (clear), 1 (cloud), 4 (unknown).
        //
        // Plan:
        //   1. Dump ALL callers of ParseLayerFile (no limit).
        //   2. Dump FUN_004b3480 (the post-ParseLayerFile call).
        //   3. Search for the string "layer" (or ".LAY") in rodata and find
        //      the function(s) that reference it — the MM parser keyword table.
        //   4. Dump GetLayerByIndex (FUN_004b3170) and ALL its callers,
        //      since the slot index likely maps to GetLayerByIndex(slot).

        // --- 1. All callers of ParseLayerFile ---
        out.println("// === All callers of ParseLayerFile (FUN_004b4370) ===");
        Address parseLayerAddr = toAddr(0x004b4370L);
        dumpAllCallers(parseLayerAddr, "ParseLayerFile");

        // --- 2. FUN_004b3480 ---
        out.println("\n// === FUN_004b3480 (called after ParseLayerFile, arg=0 in known callers) ===");
        dumpAt(0x004b3480L);
        out.println("\n// === Callers of FUN_004b3480 ===");
        dumpAllCallers(toAddr(0x004b3480L), "FUN_004b3480");

        // --- 3. String search for "layer" keyword in rodata ---
        out.println("\n// === String search: '.LAY' and 'layer' in rodata ===");
        searchStrings(new String[]{"layer", ".LAY", "LAY "});

        // --- 4. GetLayerByIndex and its callers ---
        out.println("\n// === GetLayerByIndex (FUN_004b3170) ===");
        dumpAt(0x004b3170L);
        out.println("\n// === Callers of GetLayerByIndex (FUN_004b3170) ===");
        dumpAllCallers(toAddr(0x004b3170L), "GetLayerByIndex");

        out.close();
        decompiler.dispose();
        println("Output: C:\\Temp\\mm_layer_slot.txt");
    }

    private void searchStrings(String[] patterns) throws Exception {
        Memory mem = currentProgram.getMemory();
        for (MemoryBlock block : mem.getBlocks()) {
            if (!block.isInitialized()) continue;
            String name = block.getName();
            // Only search read-only/data sections (not CODE, not BSS)
            if (!block.isRead() || block.isWrite() && name.equals(".bss")) continue;
            long start = block.getStart().getOffset();
            long end   = block.getEnd().getOffset();
            byte[] data;
            try {
                data = new byte[(int)(end - start + 1)];
                block.getBytes(block.getStart(), data);
            } catch (Exception e) {
                continue;
            }
            for (String pat : patterns) {
                byte[] patBytes = pat.getBytes("US-ASCII");
                outer:
                for (int i = 0; i <= data.length - patBytes.length; i++) {
                    for (int j = 0; j < patBytes.length; j++) {
                        if (data[i+j] != patBytes[j]) continue outer;
                    }
                    long va = start + i;
                    // Collect up to 64 bytes as string context
                    StringBuilder sb = new StringBuilder();
                    for (int k = i; k < Math.min(i+64, data.length); k++) {
                        byte b = data[k];
                        if (b == 0) break;
                        sb.append((char)(b & 0xff));
                    }
                    out.println("// STR \"" + pat + "\" @ 0x" + Long.toHexString(va)
                        + " [" + name + "]: \"" + sb + "\"");
                    // Find xrefs to this VA
                    Address strAddr = currentProgram.getAddressFactory()
                        .getDefaultAddressSpace().getAddress(va);
                    ReferenceIterator refs = currentProgram.getReferenceManager()
                        .getReferencesTo(strAddr);
                    while (refs.hasNext()) {
                        Reference ref = refs.next();
                        Function fn = currentProgram.getFunctionManager()
                            .getFunctionContaining(ref.getFromAddress());
                        if (fn != null) {
                            out.println("//   xref from " + fn.getName()
                                + " @ 0x" + Long.toHexString(fn.getEntryPoint().getOffset())
                                + " (insn 0x" + Long.toHexString(ref.getFromAddress().getOffset()) + ")");
                        }
                    }
                }
            }
        }
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
