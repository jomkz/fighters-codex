import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import java.io.*;
import java.util.*;

// DumpPROJDispatch.java
// Find the per-tick projectile physics update path to map PROJ_TYPE+0x50-0x6E.
//
// Background:
//   - PROJ_TYPE base = entity + 0xa6; gap = entity + 0xF6 to entity + 0x114.
//   - Prior scripts (DumpPROJPhysics3) scanned for offsets 0x50-0x7F from PROJ_TYPE
//     but found nothing. The entity-relative offsets (0xF6-0x114) were NOT scanned.
//   - _PROJProc @ 0x4C1F50 and ?PROJMoveProc @ 0x4C11B0 were previously decompiled;
//     neither accessed the gap bytes directly.
//   - The physics update may be dispatched via a virtual function pointer in the live
//     projectile record (like a vtable call), bypassing the direct symbol.
//
// New strategies:
//   1. Scan ALL instructions (whole binary) for entity-relative offsets 0xF6-0x114
//      (decimal 246-276) as immediate operands — these are the gap bytes.
//   2. Decompile all functions found, plus their callers (one level deep).
//   3. Force-decompile _PROJProc at 0x4C1F50, and the full 0x4C0000-0x4C3000 range.
//   4. Also scan for indirect calls (call [reg+offset] patterns) in 0x4C0000-0x4D0000
//      that might be the virtual dispatch.
//
// Run: analyzeHeadless <project> <name> -postScript DumpPROJDispatch.java
//      Output: C:\Temp\proj_dispatch.txt

public class DumpPROJDispatch extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;
    private Set<Long> dumped = new LinkedHashSet<>();

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\proj_dispatch.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        AddressFactory af = currentProgram.getAddressFactory();
        AddressSpace ds = af.getDefaultAddressSpace();
        FunctionManager fm = currentProgram.getFunctionManager();
        Listing listing = currentProgram.getListing();

        out.println("// === PROJ_TYPE gap bytes RE (PROJ_TYPE+0x50-0x6E = entity+0xF6-0x114) ===");
        out.println();

        // -------------------------------------------------------
        // PART 1: Scan whole binary for entity-relative offsets 0xF6-0x114
        //         (PROJ_TYPE+0x50 through PROJ_TYPE+0x6E)
        // -------------------------------------------------------
        out.println("// === PART 1: Scalar offsets 0xF6-0x114 (246-276) in whole binary ===");
        Set<Long> gapFuncs = new LinkedHashSet<>();
        InstructionIterator insns = listing.getInstructions(ds.getAddress(0x00400000L), true);
        while (insns.hasNext()) {
            Instruction insn = insns.next();
            if (insn.getAddress().getOffset() > 0x00600000L) break;
            for (int i = 0; i < insn.getNumOperands(); i++) {
                for (Object o : insn.getOpObjects(i)) {
                    if (o instanceof ghidra.program.model.scalar.Scalar) {
                        long val = ((ghidra.program.model.scalar.Scalar)o).getUnsignedValue();
                        if (val >= 0xF6L && val <= 0x114L) {
                            Function f = fm.getFunctionContaining(insn.getAddress());
                            if (f != null && gapFuncs.add(f.getEntryPoint().getOffset())) {
                                out.println("// offset 0x" + Long.toHexString(val)
                                        + " at " + insn.getAddress()
                                        + " in " + f.getName() + " @ " + f.getEntryPoint());
                            }
                        }
                    }
                }
            }
        }
        out.println("// Functions using entity+0xF6-0x114 offsets: " + gapFuncs.size());
        out.println();
        for (long va : gapFuncs) dumpAt(va);

        // -------------------------------------------------------
        // PART 2: Force-decompile _PROJProc and neighbors
        // -------------------------------------------------------
        out.println("// === PART 2: _PROJProc and PROJ cluster (0x4C0000-0x4C3000) ===");
        long[] projAddrs = {
            0x004C1F50L, // _PROJProc (FA.SMS)
            0x004C11B0L, // ?PROJMoveProc (not found in prior sessions — try again)
            0x004C1120L, // _PROJSpeed@8
            0x004C2170L, // _PROJFire@16
            0x004C1170L, // _PROJEngineState@0
            0x004C0500L, // start of physics cluster scanned previously
            0x004C0600L, 0x004C0700L, 0x004C0800L, 0x004C0900L,
            0x004C0A00L, 0x004C0B00L, 0x004C0C00L, 0x004C0D00L,
            0x004C0E00L, 0x004C0F00L, 0x004C1000L, 0x004C1100L,
            0x004C1200L, 0x004C1300L, 0x004C1400L, 0x004C1500L,
            0x004C1600L, 0x004C1700L, 0x004C1800L, 0x004C1900L,
            0x004C1A00L, 0x004C1B00L, 0x004C1C00L, 0x004C1D00L,
            0x004C1E00L, 0x004C2000L,
        };
        for (long va : projAddrs) {
            Address a = ds.getAddress(va);
            Function f = fm.getFunctionAt(a);
            if (f == null) f = fm.getFunctionContaining(a);
            if (f != null && !dumped.contains(f.getEntryPoint().getOffset())) {
                dumpAt(f.getEntryPoint().getOffset());
            } else if (f == null) {
                out.println("// no function at 0x" + Long.toHexString(va));
            }
        }

        // -------------------------------------------------------
        // PART 3: Functions in 0x4C0000-0x4C3000 range (all of them)
        // -------------------------------------------------------
        out.println("// === PART 3: All functions in 0x4C0000-0x4C3000 ===");
        FunctionIterator projRange = fm.getFunctions(ds.getAddress(0x004C0000L), true);
        while (projRange.hasNext()) {
            Function f = projRange.next();
            if (f.getEntryPoint().getOffset() > 0x004C3000L) break;
            dumpAt(f.getEntryPoint().getOffset());
        }
        out.println();

        // -------------------------------------------------------
        // PART 4: JT warhead flag bits 1-3, 5-6 (bits 0x2,0x4,0x8,0x20,0x40)
        //         in 0x4C0000-0x4D0000
        // -------------------------------------------------------
        out.println("// === PART 4: Warhead flag bit tests (bits 1-3,5-6) in 0x4C0000-0x4D0000 ===");
        long[] wBits = { 0x2L, 0x4L, 0x8L, 0x20L, 0x40L };
        for (long mask : wBits) {
            InstructionIterator wi = listing.getInstructions(ds.getAddress(0x004C0000L), true);
            out.println("// -- mask 0x" + Long.toHexString(mask) + " --");
            while (wi.hasNext()) {
                Instruction instr = wi.next();
                if (instr.getAddress().getOffset() > 0x004D0000L) break;
                String mn = instr.getMnemonicString().toLowerCase();
                if (!mn.equals("and") && !mn.equals("test")) continue;
                for (int i = 0; i < instr.getNumOperands(); i++) {
                    for (Object o : instr.getOpObjects(i)) {
                        if (o instanceof ghidra.program.model.scalar.Scalar) {
                            long val = ((ghidra.program.model.scalar.Scalar)o).getUnsignedValue();
                            if (val == mask) {
                                Function f = fm.getFunctionContaining(instr.getAddress());
                                if (f != null) {
                                    out.println("//   in " + f.getName() + " @ " + f.getEntryPoint()
                                            + " at " + instr.getAddress());
                                    dumpAt(f.getEntryPoint().getOffset());
                                }
                            }
                        }
                    }
                }
            }
        }

        out.close();
        decompiler.dispose();
        println("Output: C:\\Temp\\proj_dispatch.txt");
    }

    private void dumpAt(long va) throws Exception {
        if (dumped.contains(va)) return;
        dumped.add(va);
        Address addr = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(va);
        Function fn = currentProgram.getFunctionManager().getFunctionAt(addr);
        if (fn == null) fn = currentProgram.getFunctionManager().getFunctionContaining(addr);
        if (fn == null) {
            DisassembleCommand disCmd = new DisassembleCommand(addr, null, true);
            disCmd.applyTo(currentProgram, monitor);
            CreateFunctionCmd createCmd = new CreateFunctionCmd(addr);
            createCmd.applyTo(currentProgram, monitor);
            fn = getFunctionAt(addr);
        }
        if (fn == null) {
            out.println("// NOT FOUND at 0x" + Long.toHexString(va));
            return;
        }
        out.println("// --- " + fn.getName() + " @ " + fn.getEntryPoint() + " ---");
        DecompileResults res = decompiler.decompileFunction(fn, 120, monitor);
        if (res != null && res.getDecompiledFunction() != null) {
            out.println(res.getDecompiledFunction().getC());
        } else {
            out.println("// decompile failed: " + fn.getName());
        }
        out.println();
    }
}
