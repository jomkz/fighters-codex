import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class DumpLAYRemaining extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;
    private Set<Long> dumped = new LinkedHashSet<>();

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\lay_remaining.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // ---------------------------------------------------------------
        // Goal A: understand colour entry table / gradient sub-block format
        //   FindNearestColorEntry = FUN_004b3ad0
        //   It reads from DAT_00580e1c (colour entry table VA, header +0x6C)
        //   We need to understand the entry stride and field layout so we
        //   can decode the 8-byte header (31 00 00 00 / 00 00 / 10 / 10)
        // ---------------------------------------------------------------
        out.println("// === FindNearestColorEntry (FUN_004b3ad0) ===");
        dumpAt(0x004b3ad0L);

        out.println("\n// === Callers of FindNearestColorEntry ===");
        dumpAllCallers(toAddr(0x004b3ad0L), "FindNearestColorEntry");

        // ---------------------------------------------------------------
        // Goal B: survey all xrefs to the six gap DLL header globals:
        //   Header +0x34 = DAT_00580de4
        //   Header +0x38 = DAT_00580de8
        //   Header +0x3C = DAT_00580dec
        //   Header +0x60 = DAT_00580e10
        //   Header +0x64 = DAT_00580e14
        //   Header +0x68 = DAT_00580e18
        // ---------------------------------------------------------------
        long[] gapGlobals = {
            0x00580de4L, 0x00580de8L, 0x00580decL,
            0x00580e10L, 0x00580e14L, 0x00580e18L
        };
        String[] gapNames = {
            "DAT_00580de4 (header+0x34)", "DAT_00580de8 (header+0x38)", "DAT_00580dec (header+0x3C)",
            "DAT_00580e10 (header+0x60)", "DAT_00580e14 (header+0x64)", "DAT_00580e18 (header+0x68)"
        };
        out.println("\n// ===== Header gap global xref survey =====");
        for (int i = 0; i < gapGlobals.length; i++) {
            Address addr = currentProgram.getAddressFactory()
                .getDefaultAddressSpace().getAddress(gapGlobals[i]);
            ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(addr);
            List<Reference> refList = new ArrayList<>();
            while (refs.hasNext()) refList.add(refs.next());
            out.println("\n// " + gapNames[i] + ": " + refList.size() + " xrefs");
            for (Reference ref : refList) {
                out.println("//   from 0x" + Long.toHexString(ref.getFromAddress().getOffset())
                    + "  type=" + ref.getReferenceType());
                Function fn = currentProgram.getFunctionManager()
                    .getFunctionContaining(ref.getFromAddress());
                if (fn != null) {
                    out.println("//   in fn " + fn.getName()
                        + " @ 0x" + Long.toHexString(fn.getEntryPoint().getOffset()));
                    dumpAt(fn.getEntryPoint().getOffset());
                }
            }
            if (refList.isEmpty()) {
                out.println("//   (no direct xrefs)");
            }
        }

        // ---------------------------------------------------------------
        // Goal C: look for pointer-arithmetic patterns -- any function that
        // loads from the sky_layer_array base (DAT_00580dc8 / +0x18) or
        // below_layer_array base (DAT_00580df4 / +0x44) and indexes beyond
        // the 7 known entries (i.e., uses index >= 7, i.e. offset >= 0x1C).
        //
        // SetActiveLayerByAngle = FUN_004cc4b4 already shown -- check if
        // any other function indexes these arrays.
        // ---------------------------------------------------------------
        out.println("\n// ===== Callers / siblings of SetActiveLayerByAngle (FUN_004cc4b4) =====");
        dumpAllCallers(toAddr(0x004cc4b4L), "SetActiveLayerByAngle");

        // Also check DAT_00580dc8 (sky_layer_array[0]) xrefs broadly
        out.println("\n// ===== Xrefs to DAT_00580dc8 (sky_layer_array[0]) =====");
        {
            Address addr = currentProgram.getAddressFactory()
                .getDefaultAddressSpace().getAddress(0x00580dc8L);
            ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(addr);
            Set<Long> fnsSeen = new LinkedHashSet<>();
            while (refs.hasNext()) {
                Reference ref = refs.next();
                Function fn = currentProgram.getFunctionManager()
                    .getFunctionContaining(ref.getFromAddress());
                if (fn == null) continue;
                long va = fn.getEntryPoint().getOffset();
                if (fnsSeen.contains(va)) continue;
                fnsSeen.add(va);
                out.println("//   fn " + fn.getName() + " @ 0x" + Long.toHexString(va));
                dumpAt(va);
            }
        }

        out.println("\n// ===== Xrefs to DAT_00580df4 (below_layer_array[0]) =====");
        {
            Address addr = currentProgram.getAddressFactory()
                .getDefaultAddressSpace().getAddress(0x00580df4L);
            ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(addr);
            Set<Long> fnsSeen = new LinkedHashSet<>();
            while (refs.hasNext()) {
                Reference ref = refs.next();
                Function fn = currentProgram.getFunctionManager()
                    .getFunctionContaining(ref.getFromAddress());
                if (fn == null) continue;
                long va = fn.getEntryPoint().getOffset();
                if (fnsSeen.contains(va)) continue;
                fnsSeen.add(va);
                out.println("//   fn " + fn.getName() + " @ 0x" + Long.toHexString(va));
                dumpAt(va);
            }
        }

        out.close();
        decompiler.dispose();
        println("Output: C:\\Temp\\lay_remaining.txt");
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
            out.println("// (already dumped: 0x" + Long.toHexString(va) + ")");
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
