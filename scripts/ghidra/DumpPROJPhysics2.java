import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.scalar.*;
import ghidra.program.model.lang.*;
import java.io.*;
import java.util.*;

public class DumpPROJPhysics2 extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;
    private Set<Long> dumped = new LinkedHashSet<>();

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\proj_physics2.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // Goal 1: Map PROJ_TYPE+0x50-0x78 (missile+0xF6 to missile+0x11E at runtime).
        // _PROJProc is the physics update callback; _PROJLock@24=0x004c2f20,
        // _PROJHitChance@28=0x004c3380. Look for _PROJProc by symbol name.
        out.println("// === Searching for _PROJProc by symbol name ===");
        SymbolTable symTable = currentProgram.getSymbolTable();
        SymbolIterator syms = symTable.getSymbols("_PROJProc");
        boolean found = false;
        while (syms.hasNext()) {
            Symbol sym = syms.next();
            out.println("// Found _PROJProc @ " + sym.getAddress());
            dumpAt(sym.getAddress().getOffset());
            found = true;
        }
        if (!found) {
            // Also try without underscore and with stdcall mangling
            String[] candidates = {"PROJProc", "_PROJProc@4", "_PROJProc@8",
                                   "_PROJUpdate", "_PROJTick"};
            for (String name : candidates) {
                SymbolIterator si = symTable.getSymbols(name);
                while (si.hasNext()) {
                    Symbol sym = si.next();
                    out.println("// Found " + name + " @ " + sym.getAddress());
                    dumpAt(sym.getAddress().getOffset());
                    found = true;
                }
            }
        }
        if (!found) {
            out.println("// _PROJProc not found by name — searching near known PROJ functions");
            // Known PROJ functions: _PROJLock@24=0x004c2f20, _PROJHitChance@28=0x004c3380
            // Scan 0x004c0000-0x004c8000 for functions that read missile+0xF6..0x11E
            out.println("// Scanning for functions reading offsets 0xF6-0x11E from pointer");
            searchReadsInRange(0x004c0000L, 0x004c8000L, 0xF6, 0x11E);
        }

        // Goal 2: Map warhead flags bits 1-3 (0x02/0x04/0x08) and 5-6 (0x20/0x40).
        // Known: bit 0=0x01 (missile), bit 4=0x10, bit 7=0x80 (gun).
        // Remaining: 0x02, 0x04, 0x08, 0x20, 0x40 in the lower byte.
        // Search for AND/TEST with these masks in PROJ-related code range.
        out.println("\n// === Searching for warhead-flag bit tests 0x02/0x04/0x08/0x20/0x40 ===");
        int[] masks = {0x02, 0x04, 0x08, 0x20, 0x40};
        for (int mask : masks) {
            out.println("// -- mask 0x" + Integer.toHexString(mask) + " --");
            searchBitTestsInRange(0x004b0000L, 0x004d0000L, mask);
        }

        // Also dump _PROJHitChance@28 for cross-reference context on bits 4/21.
        out.println("\n// === _PROJHitChance@28 (0x004c3380) for cross-reference ===");
        dumpAt(0x004c3380L);

        // And _DAMAGEDoHit@12 hit-sound dispatch to cross-check bits 0 and 7.
        out.println("\n// === _DAMAGEDoHit@12 (0x0040f970) hit-sound dispatch ===");
        dumpAt(0x0040f970L);

        out.close();
        decompiler.dispose();
        println("Output: C:\\Temp\\proj_physics2.txt");
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

    private void searchReadsInRange(long start, long end, int minOff, int maxOff) throws Exception {
        AddressSpace space = currentProgram.getAddressFactory().getDefaultAddressSpace();
        Listing listing = currentProgram.getListing();
        Address startAddr = space.getAddress(start);
        Address endAddr = space.getAddress(end);
        Set<Long> seen = new LinkedHashSet<>();

        InstructionIterator instrs = listing.getInstructions(startAddr, true);
        while (instrs.hasNext()) {
            Instruction instr = instrs.next();
            if (instr.getAddress().compareTo(endAddr) > 0) break;
            // Look for memory references with offsets in [minOff, maxOff]
            for (int i = 0; i < instr.getNumOperands(); i++) {
                Object[] objs = instr.getOpObjects(i);
                for (Object obj : objs) {
                    if (obj instanceof Scalar) {
                        long val = ((Scalar) obj).getUnsignedValue();
                        if (val >= minOff && val <= maxOff) {
                            Function fn = currentProgram.getFunctionManager()
                                    .getFunctionContaining(instr.getAddress());
                            if (fn != null && !seen.contains(fn.getEntryPoint().getOffset())) {
                                seen.add(fn.getEntryPoint().getOffset());
                                out.println("// offset 0x" + Long.toHexString(val)
                                        + " in " + fn.getName()
                                        + " @ " + fn.getEntryPoint());
                            }
                        }
                    }
                }
            }
        }
        // Dump all found functions
        for (long va : seen) {
            dumpAt(va);
        }
    }

    private void searchBitTestsInRange(long start, long end, int mask) throws Exception {
        AddressSpace space = currentProgram.getAddressFactory().getDefaultAddressSpace();
        Listing listing = currentProgram.getListing();
        Address startAddr = space.getAddress(start);
        Address endAddr = space.getAddress(end);

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
                            String fnName = fn != null ? fn.getName() : "unknown";
                            out.println("  " + instr.getAddress() + "  "
                                    + instr.getMnemonicString() + "  "
                                    + instr.getDefaultOperandRepresentation(0)
                                    + ", " + instr.getDefaultOperandRepresentation(1)
                                    + "  [in " + fnName + "]");
                        }
                    }
                }
            }
        }
    }
}
