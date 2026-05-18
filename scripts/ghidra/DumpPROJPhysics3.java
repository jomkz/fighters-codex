import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.scalar.*;
import java.io.*;
import java.util.*;

public class DumpPROJPhysics3 extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;
    private Set<Long> dumped = new LinkedHashSet<>();

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\proj_physics3.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // Goal 1: map PROJ_TYPE gap bytes 0x50-0x6E (turn rate, g-limit, flight model params).
        // _PROJProc is at 0x004C1F50 (confirmed from FA.SMS). PROJMoveProc at 0x004C11B0.
        out.println("// === _PROJProc @ 0x004C1F50 ===");
        dumpAt(0x004C1F50L);

        out.println("\n// === ?PROJMoveProc @ 0x004C11B0 ===");
        dumpAt(0x004C11B0L);

        out.println("\n// === _PROJSpeed@8 @ 0x004C1120 ===");
        dumpAt(0x004C1120L);

        out.println("\n// === _PROJFire@16 @ 0x004C2170 ===");
        dumpAt(0x004C2170L);

        out.println("\n// === _PROJEngineState@0 @ 0x004C1170 ===");
        dumpAt(0x004C1170L);

        // Scan for functions reading PROJ_TYPE at offsets 0x50-0x6F (the physics gap).
        // These are the turn rate, g-limit, and related flight model parameters.
        out.println("\n// === Functions reading PROJ_TYPE offsets 0x50-0x7F ===");
        Set<Long> physFns = findFunctionsReadingOffsets(0x00400000L, 0x00500000L, 0x50, 0x7F);
        out.println("// Found " + physFns.size() + " functions");
        for (long va : physFns) {
            dumpAt(va);
        }

        // Goal 2: JT warhead flag bits 1-3, 5-6 (at missile+0xa6 / PROJ_TYPE+0xa6).
        // Scan for bit tests against 0x2, 0x4, 0x8, 0x20, 0x40 in the PROJ code range.
        out.println("\n// === Warhead flag bit tests (bits 1-3, 5-6) in 0x004C0000-0x004D0000 ===");
        long[] wBits = { 0x2L, 0x4L, 0x8L, 0x20L, 0x40L };
        for (long mask : wBits) {
            Set<Long> fns = findFunctionsWithMask(0x004C0000L, 0x004D0000L, mask);
            out.println("// -- mask 0x" + Long.toHexString(mask) + " --");
            for (long va : fns) {
                out.println("//   found in " + getFunctionName(va));
                dumpAt(va);
            }
        }

        out.close();
        decompiler.dispose();
        println("Output: C:\\Temp\\proj_physics3.txt");
    }

    private String getFunctionName(long va) {
        Address addr = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(va);
        Function fn = currentProgram.getFunctionManager().getFunctionAt(addr);
        return fn != null ? fn.getName() + " @ 0x" + Long.toHexString(va) : "0x" + Long.toHexString(va);
    }

    private Set<Long> findFunctionsReadingOffsets(long start, long end, int minOff, int maxOff) throws Exception {
        AddressSpace space = currentProgram.getAddressFactory().getDefaultAddressSpace();
        Listing listing = currentProgram.getListing();
        Address startAddr = space.getAddress(start);
        Address endAddr = space.getAddress(end);
        Set<Long> seen = new LinkedHashSet<>();

        InstructionIterator instrs = listing.getInstructions(startAddr, true);
        while (instrs.hasNext()) {
            Instruction instr = instrs.next();
            if (instr.getAddress().compareTo(endAddr) > 0) break;
            for (int i = 0; i < instr.getNumOperands(); i++) {
                Object[] objs = instr.getOpObjects(i);
                for (Object obj : objs) {
                    if (obj instanceof Scalar) {
                        long val = ((Scalar) obj).getUnsignedValue();
                        if (val >= minOff && val <= maxOff) {
                            Function fn = currentProgram.getFunctionManager()
                                    .getFunctionContaining(instr.getAddress());
                            if (fn != null) {
                                long fva = fn.getEntryPoint().getOffset();
                                if (seen.add(fva)) {
                                    out.println("// offset 0x" + Long.toHexString(val)
                                            + " in " + fn.getName() + " @ " + fn.getEntryPoint());
                                }
                            }
                        }
                    }
                }
            }
        }
        return seen;
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
                                seen.add(fva);
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
