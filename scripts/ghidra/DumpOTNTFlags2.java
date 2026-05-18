import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.scalar.*;
import java.io.*;
import java.util.*;

public class DumpOTNTFlags2 extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;
    private Set<Long> dumped = new LinkedHashSet<>();

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\otnt_flags2.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // Goal: confirm OT/NT ot_flags bit semantics.
        // ot_flags is a dword at OBJ_TYPE+0x09 = entity+0x09 in the runtime struct.
        // Known confirmed: bits 0 (targetable), 4 (naval), 17 (hardened military - OT only).
        // Pending confirmation (from category inference): bits 5,8,9,10,11,15,18,19,20,22,25,26.
        //
        // Strategy: search for AND/TEST with each target mask, collect containing functions,
        // decompile the most relevant ones. Focus on:
        //   - carrier approach code (0x004900-0x004a00) for bits 15,18,19,20
        //   - targeting/collision code for bits 8,10,11
        //   - AI/unit class code for bits 5,9,25,26

        // OT bit 22 (0x400000) and NT bits 25,26 (0x2000000, 0x4000000) are unusual large values.
        // Search whole binary for these first.
        long[] priorityMasks = {
            0x8000L,    // bit 15 — flight deck
            0x40000L,   // bit 18 — arrestor wire
            0x80000L,   // bit 19 — catapult
            0x100000L,  // bit 20 — VSTOL deck
            0x400000L,  // bit 22 — large variant (OT)
            0x2000000L, // bit 25 — emplaced AA
            0x4000000L, // bit 26 — SA-2A marker
        };

        long[] commonMasks = {
            0x20L,   // bit 5 — armed
            0x100L,  // bit 8 — has geometry
            0x200L,  // bit 9 — large platform
            0x400L,  // bit 10 — civilian/light
            0x800L,  // bit 11 — animated/ground-mobile
        };

        out.println("// === Priority bit masks (carrier/special) — full binary scan ===");
        for (long mask : priorityMasks) {
            out.println("\n// -- mask 0x" + Long.toHexString(mask) + " (bit " + Long.numberOfTrailingZeros(mask) + ") --");
            Set<Long> found = findFunctionsWithMask(0x00400000L, 0x00600000L, mask);
            for (long va : found) {
                dumpAt(va);
            }
        }

        out.println("\n// === Common bit masks — focused scan 0x004b0000-0x004f0000 ===");
        for (long mask : commonMasks) {
            out.println("\n// -- mask 0x" + Long.toHexString(mask) + " (bit " + Long.numberOfTrailingZeros(mask) + ") --");
            Set<Long> found = findFunctionsWithMask(0x004b0000L, 0x004f0000L, mask);
            for (long va : found) {
                dumpAt(va);
            }
        }

        // Also dump known carrier-approach function for bit 15/18/19 context.
        out.println("\n// === FUN_004bed70 (carrier finder, known reader of ot_flags) ===");
        dumpAt(0x004bed70L);

        // FUN_004747c0 referenced in fire-control / lock context — likely tests ot_flags.
        out.println("\n// === FUN_004747c0 (entity live-check / capability gate) ===");
        dumpAt(0x004747c0L);

        // Hardpoint bit 1 ($2) — search fire-control dispatch (_GVProc area 0x00463xxx)
        out.println("\n// === Hardpoint bit $2 tests in 0x00460000-0x00480000 ===");
        Set<Long> hpFns = findFunctionsWithMask(0x00460000L, 0x00480000L, 0x2L);
        for (long va : hpFns) {
            dumpAt(va);
        }

        out.close();
        decompiler.dispose();
        println("Output: C:\\Temp\\otnt_flags2.txt");
    }

    private Set<Long> findFunctionsWithMask(long start, long end, long mask) throws Exception {
        AddressSpace space = currentProgram.getAddressFactory().getDefaultAddressSpace();
        Listing listing = currentProgram.getListing();
        Address startAddr = space.getAddress(start);
        Address endAddr = space.getAddress(end);
        Set<Long> seen = new LinkedHashSet<>();

        InstructionIterator instrs = listing.getInstructions(startAddr, true);
        while (instrs.hasNext()) {
            Instruction instr = instrs.next();
            if (instr.getAddress().compareTo(endAddr) > 0) break;
            String mnem = instr.getMnemonicString().toLowerCase();
            if (!mnem.equals("and") && !mnem.equals("test")) continue;
            for (int i = 0; i < instr.getNumOperands(); i++) {
                Object[] objs = instr.getOpObjects(i);
                for (Object obj : objs) {
                    if (obj instanceof Scalar) {
                        long val = ((Scalar) obj).getUnsignedValue();
                        if (val == mask) {
                            Function fn = currentProgram.getFunctionManager()
                                    .getFunctionContaining(instr.getAddress());
                            if (fn != null) {
                                long fva = fn.getEntryPoint().getOffset();
                                if (seen.add(fva)) {
                                    out.println("  // 0x" + Long.toHexString(mask)
                                            + " found at " + instr.getAddress()
                                            + " in " + fn.getName());
                                }
                            }
                        }
                    }
                }
            }
        }
        return seen;
    }

    private void dumpAt(long va) throws Exception {
        if (dumped.contains(va)) return;
        dumped.add(va);
        Address addr = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(va);
        Function fn = currentProgram.getFunctionManager().getFunctionAt(addr);
        if (fn == null) {
            out.println("// NOT FOUND at 0x" + Long.toHexString(va));
            return;
        }
        out.println("// --- " + fn.getName() + " @ 0x" + Long.toHexString(va) + " ---");
        DecompileResults res = decompiler.decompileFunction(fn, 60, monitor);
        if (res != null && res.getDecompiledFunction() != null) {
            out.println(res.getDecompiledFunction().getC());
        } else {
            out.println("// decompile failed: " + fn.getName());
        }
    }
}
