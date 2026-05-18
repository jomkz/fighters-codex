// DumpHUDBit14Search.java
// Find the writer of HUD advisory bit 14 (0x4000) in DAT_0050cfef.
//
// Strategy:
//   1. Find all callers of FUN_00452140 (the function that ORs PLANECheckFuel result into DAT_0050cfef)
//      and dump each caller's decompile — one of these may also write 0x4000.
//   2. Search all functions for any instruction that stores 0x4000 or 0x00004000
//      into memory at or near DAT_0050cfef (0x0050cfef).
//   3. Also dump any reference to 0x0050cfef that is a WRITE (not a read).
//
// Prior work (prior Ghidra session):
//   - FUN_0049fb70 (_PLANECheckFuel@0) confirmed: only returns 0/0x8000/0x10000/0x20000/0x40000
//     (never 0x4000). Bit 14 must come from a separate writer.
//   - FUN_00452140 is the caller of FUN_0049fb70 that ORs the result into DAT_0050cfef.
//
// Run: analyzeHeadless <project> <name> -postScript DumpHUDBit14Search.java
//      Output: C:\Temp\hud_bit14_search.txt

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.scalar.*;
import ghidra.program.model.symbol.*;
import ghidra.app.decompiler.*;
import ghidra.util.task.TaskMonitor;
import java.io.*;
import java.util.*;

public class DumpHUDBit14Search extends GhidraScript {

    private PrintWriter out;

    @Override
    public void run() throws Exception {
        File outFile = new File("C:\\Temp\\hud_bit14_search.txt");
        out = new PrintWriter(new FileWriter(outFile));
        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);

        AddressFactory af = currentProgram.getAddressFactory();
        AddressSpace defaultSpace = af.getDefaultAddressSpace();
        FunctionManager fm = currentProgram.getFunctionManager();

        // Target addresses
        long ADDR_FUN452140 = 0x00452140L;
        long ADDR_DAT_CFEF  = 0x0050cfefL;
        long BIT14          = 0x4000L;

        out.println("=== HUD bit 14 (0x4000) writer search ===");
        out.println("DAT_0050cfef = 0x" + Long.toHexString(ADDR_DAT_CFEF));
        out.println("Searching for 0x4000 stores and callers of FUN_00452140");
        out.println();

        // -------------------------------------------------------
        // PART 1: Callers of FUN_00452140
        // -------------------------------------------------------
        out.println("=== PART 1: Callers of FUN_00452140 ===");
        Address addr452140 = defaultSpace.getAddress(ADDR_FUN452140);
        Function fun452140 = fm.getFunctionAt(addr452140);
        if (fun452140 == null) {
            out.println("FUN_00452140 not found as function entry — searching references");
        }

        // Get all call references TO FUN_00452140
        ReferenceManager rm = currentProgram.getReferenceManager();
        ReferenceIterator refs = rm.getReferencesTo(addr452140);
        List<Address> callerAddrs = new ArrayList<>();
        while (refs.hasNext()) {
            Reference ref = refs.next();
            if (ref.getReferenceType().isCall()) {
                callerAddrs.add(ref.getFromAddress());
            }
        }
        out.println("Found " + callerAddrs.size() + " callers of FUN_00452140");
        out.println();

        Set<Function> callerFuncs = new HashSet<>();
        for (Address callSite : callerAddrs) {
            Function caller = fm.getFunctionContaining(callSite);
            if (caller != null) callerFuncs.add(caller);
        }

        for (Function caller : callerFuncs) {
            out.println("--- Caller: " + caller.getName() + " @ " + caller.getEntryPoint() + " ---");
            DecompileResults dr = decomp.decompileFunction(caller, 60, TaskMonitor.DUMMY);
            if (dr.decompileCompleted()) {
                out.println(dr.getDecompiledFunction().getC());
            } else {
                out.println("  [decompile failed: " + dr.getErrorMessage() + "]");
            }
            out.println();
        }

        // -------------------------------------------------------
        // PART 2: All references (writes) to DAT_0050cfef
        // -------------------------------------------------------
        out.println("=== PART 2: Write references to DAT_0050cfef ===");
        Address addrCfef = defaultSpace.getAddress(ADDR_DAT_CFEF);
        ReferenceIterator cfefRefs = rm.getReferencesTo(addrCfef);
        Set<Function> cfefWriters = new HashSet<>();
        int writeCount = 0;
        while (cfefRefs.hasNext()) {
            Reference ref = cfefRefs.next();
            if (ref.getReferenceType().isWrite() || ref.getReferenceType().isData()) {
                writeCount++;
                Address from = ref.getFromAddress();
                Function f = fm.getFunctionContaining(from);
                if (f != null) cfefWriters.add(f);
                out.println("  WRITE ref at " + from + " (type=" + ref.getReferenceType() + ")"
                        + (f != null ? " in " + f.getName() : " [no function]"));
            }
        }
        out.println("Total write refs: " + writeCount + ", unique functions: " + cfefWriters.size());
        out.println();

        // Decompile each writer function not already covered in Part 1
        for (Function writer : cfefWriters) {
            if (callerFuncs.contains(writer)) continue; // already dumped
            out.println("--- Writer: " + writer.getName() + " @ " + writer.getEntryPoint() + " ---");
            DecompileResults dr = decomp.decompileFunction(writer, 60, TaskMonitor.DUMMY);
            if (dr.decompileCompleted()) {
                out.println(dr.getDecompiledFunction().getC());
            } else {
                out.println("  [decompile failed: " + dr.getErrorMessage() + "]");
            }
            out.println();
        }

        // -------------------------------------------------------
        // PART 3: Scan all instructions for 0x4000 constant stores
        //         to addresses near DAT_0050cfef (±16 bytes)
        // -------------------------------------------------------
        out.println("=== PART 3: Instructions containing 0x4000 constant ===");
        Listing listing = currentProgram.getListing();
        InstructionIterator insns = listing.getInstructions(true);
        Set<Function> bit14Funcs = new HashSet<>();
        int hit = 0;
        while (insns.hasNext()) {
            Instruction insn = insns.next();
            // Look for immediate 0x4000 in operands
            boolean has4000 = false;
            for (int i = 0; i < insn.getNumOperands(); i++) {
                Object[] objs = insn.getOpObjects(i);
                for (Object o : objs) {
                    if (o instanceof Scalar) {
                        long val = ((Scalar)o).getUnsignedValue();
                        if (val == 0x4000L || val == 0x00004000L) { has4000 = true; break; }
                    }
                }
                if (has4000) break;
            }
            if (has4000) {
                hit++;
                Function f = fm.getFunctionContaining(insn.getAddress());
                String fname = (f != null) ? f.getName() + " @ " + f.getEntryPoint() : "[no func]";
                out.println("  0x4000 at " + insn.getAddress() + " in " + fname + " : " + insn);
                if (f != null && !callerFuncs.contains(f) && !cfefWriters.contains(f)) {
                    bit14Funcs.add(f);
                }
            }
        }
        out.println("Total 0x4000 constant hits: " + hit);
        out.println();

        // Decompile any new functions that use 0x4000
        if (!bit14Funcs.isEmpty()) {
            out.println("=== PART 4: Decompile of functions using 0x4000 (not already dumped) ===");
            for (Function f : bit14Funcs) {
                out.println("--- " + f.getName() + " @ " + f.getEntryPoint() + " ---");
                DecompileResults dr = decomp.decompileFunction(f, 60, TaskMonitor.DUMMY);
                if (dr.decompileCompleted()) {
                    out.println(dr.getDecompiledFunction().getC());
                } else {
                    out.println("  [decompile failed: " + dr.getErrorMessage() + "]");
                }
                out.println();
            }
        }

        out.println("=== END ===");
        out.close();
        println("Done → C:\\Temp\\hud_bit14_search.txt");
    }
}
