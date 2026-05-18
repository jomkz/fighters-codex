import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class DumpLAYGaps extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;
    private Set<Long> dumped = new LinkedHashSet<>();

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\lay_gaps.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // Goals:
        //   1. Gradient sub-block header (0x31 and 10 10 at 0x10B0):
        //      Trace FUN_004b3410 (colour lookup using colour_array), its callers,
        //      and any function that reads the 8-byte sub-block header.
        //   2. Header gap fields (+0x14/+0x18 = DAT_00580dc4/dc8;
        //      +0x34..+0x44 = DAT_00580de4..df4; +0x60..+0x68 = DAT_00580e10..e18):
        //      Find all xrefs to these globals and dump the functions that read them.
        //   3. MM slot index (0/1/4): ParseLayerFile has one param (filename).
        //      Trace its callers to find where the slot index is used.
        //   4. FUN_004b4790 — called at end of ParseLayerFile (post-init hook).

        // --- Part 1: colour lookup and sub-block reader ---
        out.println("\n// === FUN_004b3410 (colour lookup via colour_array) ===");
        dumpAt(0x004b3410L);

        out.println("\n// === Callers of FUN_004b3410 ===");
        dumpCallers(0x004b3410L, 6);

        // --- Part 2: ParseLayerFile callers (slot index) ---
        out.println("\n// === Callers of ParseLayerFile (FUN_004b4370) ===");
        dumpCallers(0x004b4370L, 4);

        // --- Part 3: Post-init (end of ParseLayerFile) ---
        out.println("\n// === FUN_004b4790 (post-init, called by ParseLayerFile) ===");
        dumpAt(0x004b4790L);

        // --- Part 4: Gap global xrefs ---
        // Header gap fields: +0x14/+0x18 = DAT_00580dc4/dc8
        //                    +0x34..+0x44 = DAT_00580de4..df4 (5 dwords)
        //                    +0x60..+0x68 = DAT_00580e10..e18 (3 dwords)
        long[] gapGlobals = {
            0x00580dc4L, 0x00580dc8L,
            0x00580de4L, 0x00580de8L, 0x00580decL, 0x00580df0L, 0x00580df4L,
            0x00580e10L, 0x00580e14L, 0x00580e18L,
        };
        String[] gapNames = {
            "DAT_00580dc4 (+0x14)", "DAT_00580dc8 (+0x18)",
            "DAT_00580de4 (+0x34)", "DAT_00580de8 (+0x38)", "DAT_00580dec (+0x3C)",
            "DAT_00580df0 (+0x40)", "DAT_00580df4 (+0x44)",
            "DAT_00580e10 (+0x60)", "DAT_00580e14 (+0x64)", "DAT_00580e18 (+0x68)",
        };

        Set<Long> gapFnSeen = new LinkedHashSet<>();
        out.println("\n// === Gap global xref survey ===");
        for (int i = 0; i < gapGlobals.length; i++) {
            Address addr = currentProgram.getAddressFactory()
                .getDefaultAddressSpace().getAddress(gapGlobals[i]);
            ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(addr);
            boolean anyRef = false;
            while (refs.hasNext()) {
                Reference ref = refs.next();
                Address fromAddr = ref.getFromAddress();
                Function fn = currentProgram.getFunctionManager().getFunctionContaining(fromAddr);
                if (fn != null) {
                    long fnVA = fn.getEntryPoint().getOffset();
                    out.println("// " + gapNames[i] + " referenced from "
                        + fn.getName() + " @ 0x" + Long.toHexString(fnVA)
                        + " (insn 0x" + Long.toHexString(fromAddr.getOffset()) + ")");
                    gapFnSeen.add(fnVA);
                    anyRef = true;
                } else {
                    out.println("// " + gapNames[i] + " referenced from non-function @ 0x"
                        + Long.toHexString(fromAddr.getOffset()));
                    anyRef = true;
                }
            }
            if (!anyRef) {
                out.println("// " + gapNames[i] + " — no xrefs found");
            }
        }

        out.println("\n// === Functions referencing gap globals (decompiled) ===");
        for (long fnVA : gapFnSeen) {
            out.println("\n// --- gap-ref function @ 0x" + Long.toHexString(fnVA) + " ---");
            dumpAt(fnVA);
        }

        // --- Part 5: UpdateSkyState and UpdateAuroraClouds (use globals after load) ---
        out.println("\n// === UpdateSkyState @ 0x4b3d90 ===");
        dumpAt(0x004b3d90L);

        out.println("\n// === UpdateAuroraClouds @ 0x4b4170 ===");
        dumpAt(0x004b4170L);

        out.close();
        decompiler.dispose();
        println("Output: C:\\Temp\\lay_gaps.txt");
    }

    private void dumpCallers(long targetVA, int maxCallers) throws Exception {
        Address addr = currentProgram.getAddressFactory()
            .getDefaultAddressSpace().getAddress(targetVA);
        ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(addr);
        int count = 0;
        Set<Long> callersSeen = new LinkedHashSet<>();
        while (refs.hasNext() && count < maxCallers) {
            Reference ref = refs.next();
            if (ref.getReferenceType().isCall()) {
                Function caller = currentProgram.getFunctionManager()
                    .getFunctionContaining(ref.getFromAddress());
                if (caller != null) {
                    long callerVA = caller.getEntryPoint().getOffset();
                    if (!callersSeen.contains(callerVA)) {
                        callersSeen.add(callerVA);
                        out.println("\n// Caller: " + caller.getName()
                            + " @ 0x" + Long.toHexString(callerVA));
                        dumpAt(callerVA);
                        count++;
                    }
                }
            }
        }
        if (count == 0) {
            out.println("// No callers found for 0x" + Long.toHexString(targetVA));
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
