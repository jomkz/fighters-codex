import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.scalar.*;
import java.io.*;
import java.util.*;

public class DumpDLGDispatch extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;
    private Set<Long> dumped = new LinkedHashSet<>();

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\dlg_dispatch.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // Goal: confirm DLG common header gap +0x02..+0x09 role, and decode
        // _ChoosePreload four-i16 params (379, 80, 238, 361 in CHOOSEAC.DLG).
        //
        // Known: _ChoosePreload body = FUN_004897f0.
        // The gap bytes are NOT accessed by any draw function.
        // Hypothesis: used by the input dispatcher for hit-testing.
        //
        // Strategy:
        //   1. Dump FUN_004897f0 full body to check what it does with its params.
        //   2. Find functions calling FUN_004897f0 to understand param provenance.
        //   3. Search for the input dispatcher — a function that iterates the DLG
        //      CODE section and reads short/byte fields at small offsets (+2..+9)
        //      from each record pointer.
        //   4. Scan 0x00448000-0x0046C000 (UI / input / DLG area) for functions
        //      that read i16 from ptr+2 AND ptr+4 (bounding-box hit-test pattern).
        //   5. Dump the DLG main loop / mouse dispatcher if found.
        //   6. Also dump _ChooseDispatch / _DLGDispatch by name if present.

        // --- Step 1: FUN_004897f0 (_ChoosePreload) ---
        out.println("// === FUN_004897f0 (_ChoosePreload body) ===");
        dumpAt(0x004897f0L);

        // --- Step 2: callers of FUN_004897f0 ---
        out.println("\n// === Callers of FUN_004897f0 ===");
        Set<Long> callers = findCallers(0x004897f0L);
        for (long va : callers) {
            out.println("  // caller: 0x" + Long.toHexString(va));
            dumpAt(va);
        }

        // --- Step 3: search for _ChooseDispatch, _DLGDispatch, _DLGProcess by name ---
        out.println("\n// === Symbol search: Choose*/DLG* dispatch symbols ===");
        String[] searchNames = {
            "_ChooseDispatch", "_DLGDispatch", "_DLGProcess", "_ChooseProcess",
            "_DrawDispatch", "_UIDispatch", "_MenuDispatch", "_ChooseHit",
            "_DLGHit", "_HitTest"
        };
        for (String name : searchNames) {
            Function fn = findFunctionByName(name);
            if (fn != null) {
                out.println("\n// === " + name + " @ 0x" + Long.toHexString(fn.getEntryPoint().getOffset()) + " ===");
                dumpAt(fn.getEntryPoint().getOffset());
            } else {
                out.println("// NOT FOUND: " + name);
            }
        }

        // --- Step 4: scan 0x448000-0x46C000 for functions reading ptr+2 and ptr+4 ---
        // These would be hit-test functions: compare mouse x/y against stored bounding box.
        // We look for functions that have both offset-2 and offset-4 short reads
        // and also offset-6 and offset-8 (four shorts = x1, y1, x2, y2).
        out.println("\n// === Bounding-box hit-test candidates (reads +2/+4/+6/+8 in same fn) ===");
        Set<Long> hitCandidates = findHitTestFunctions(0x00448000L, 0x0046C000L);
        out.println("// Candidates found: " + hitCandidates.size());
        for (long va : hitCandidates) {
            out.println("\n// -- candidate 0x" + Long.toHexString(va) + " --");
            dumpAt(va);
        }

        // Also search the broader range 0x00440000-0x004b0000 for same pattern.
        out.println("\n// === Broader bounding-box scan 0x440000-0x4b0000 ===");
        Set<Long> hitCandidates2 = findHitTestFunctions(0x00440000L, 0x004b0000L);
        // Filter to new ones not already dumped
        for (long va : hitCandidates2) {
            if (!hitCandidates.contains(va)) {
                out.println("\n// -- candidate (broad) 0x" + Long.toHexString(va) + " --");
                dumpAt(va);
            }
        }

        // --- Step 5: dump FUN_00489840 (_ChoosePreload's asset loader, called with action_type) ---
        out.println("\n// === FUN_00489840 (action-type asset loader, called by _ChoosePreload) ===");
        dumpAt(0x00489840L);

        // --- Step 6: search for FUN that reads *dispatch record* at specific offsets ---
        // DLG dispatch record: thunk at +0, flags at +1, gap at +2..+9, x at +0xA, y at +0xC
        // Input dispatcher would compare mouse_x against record[+0xA..+0x0C] and
        // use gap bytes for identification. Search for code reading (ptr + 0xA) as i16.
        out.println("\n// === Functions reading i16 at +0xA and +0xC from same ptr (DLG x/y reader) ===");
        Set<Long> dlgReaders = findDLGXYReaders(0x00440000L, 0x004c0000L);
        out.println("// Found: " + dlgReaders.size());
        for (long va : dlgReaders) {
            if (!dumped.contains(va)) {
                out.println("\n// -- DLG x/y reader 0x" + Long.toHexString(va) + " --");
                dumpAt(va);
            }
        }

        out.close();
        decompiler.dispose();
        println("Output: C:\\Temp\\dlg_dispatch.txt");
    }

    // Find all functions that call the given target VA.
    private Set<Long> findCallers(long targetVa) throws Exception {
        AddressSpace space = currentProgram.getAddressFactory().getDefaultAddressSpace();
        Address targetAddr = space.getAddress(targetVa);
        Set<Long> callers = new LinkedHashSet<>();
        Listing listing = currentProgram.getListing();
        // Iterate references TO the target
        for (ghidra.program.model.symbol.Reference ref :
                currentProgram.getReferenceManager().getReferencesTo(targetAddr)) {
            Address fromAddr = ref.getFromAddress();
            Function fn = currentProgram.getFunctionManager().getFunctionContaining(fromAddr);
            if (fn != null) {
                callers.add(fn.getEntryPoint().getOffset());
            }
        }
        return callers;
    }

    // Find functions by name in the symbol table.
    private Function findFunctionByName(String name) throws Exception {
        SymbolTable symTable = currentProgram.getSymbolTable();
        for (Symbol sym : symTable.getSymbols(name)) {
            Address addr = sym.getAddress();
            Function fn = currentProgram.getFunctionManager().getFunctionAt(addr);
            if (fn != null) return fn;
        }
        return null;
    }

    // Find functions that read i16 at BOTH +2 AND +4 from same base register
    // in the specified address range. These are bounding-box hit-test candidates.
    private Set<Long> findHitTestFunctions(long startVa, long endVa) throws Exception {
        AddressSpace space = currentProgram.getAddressFactory().getDefaultAddressSpace();
        Listing listing = currentProgram.getListing();
        Address startAddr = space.getAddress(startVa);
        Address endAddr = space.getAddress(endVa);

        // Track: for each function, whether it has accesses at +2, +4, +6, +8
        Map<Long, Set<Integer>> fnOffsets = new LinkedHashMap<>();
        InstructionIterator instrs = listing.getInstructions(startAddr, true);
        while (instrs.hasNext()) {
            Instruction instr = instrs.next();
            if (instr.getAddress().compareTo(endAddr) > 0) break;
            String mnem = instr.getMnemonicString().toLowerCase();
            if (!mnem.startsWith("mov") && !mnem.startsWith("cmp") &&
                !mnem.startsWith("test") && !mnem.startsWith("lea")) continue;
            // Look for memory operands with small offsets
            for (int i = 0; i < instr.getNumOperands(); i++) {
                Object[] objs = instr.getOpObjects(i);
                for (Object obj : objs) {
                    if (obj instanceof Scalar) {
                        long val = ((Scalar) obj).getSignedValue();
                        if (val == 2 || val == 4 || val == 6 || val == 8) {
                            Function fn = currentProgram.getFunctionManager()
                                    .getFunctionContaining(instr.getAddress());
                            if (fn != null) {
                                long fva = fn.getEntryPoint().getOffset();
                                fnOffsets.computeIfAbsent(fva, k -> new HashSet<>()).add((int)val);
                            }
                        }
                    }
                }
            }
        }

        // Keep only functions that access at least 3 of the 4 offsets {2,4,6,8}
        Set<Long> result = new LinkedHashSet<>();
        for (Map.Entry<Long, Set<Integer>> e : fnOffsets.entrySet()) {
            Set<Integer> offsets = e.getValue();
            int count = 0;
            for (int o : new int[]{2, 4, 6, 8}) {
                if (offsets.contains(o)) count++;
            }
            if (count >= 3) {
                result.add(e.getKey());
            }
        }
        return result;
    }

    // Find functions that read i16 at both +0xA and +0xC from the same pointer
    // (DLG record x/y field readers).
    private Set<Long> findDLGXYReaders(long startVa, long endVa) throws Exception {
        AddressSpace space = currentProgram.getAddressFactory().getDefaultAddressSpace();
        Listing listing = currentProgram.getListing();
        Address startAddr = space.getAddress(startVa);
        Address endAddr = space.getAddress(endVa);

        Map<Long, Set<Integer>> fnOffsets = new LinkedHashMap<>();
        InstructionIterator instrs = listing.getInstructions(startAddr, true);
        while (instrs.hasNext()) {
            Instruction instr = instrs.next();
            if (instr.getAddress().compareTo(endAddr) > 0) break;
            String mnem = instr.getMnemonicString().toLowerCase();
            if (!mnem.startsWith("mov") && !mnem.startsWith("cmp") && !mnem.startsWith("lea")) continue;
            for (int i = 0; i < instr.getNumOperands(); i++) {
                Object[] objs = instr.getOpObjects(i);
                for (Object obj : objs) {
                    if (obj instanceof Scalar) {
                        long val = ((Scalar) obj).getSignedValue();
                        if (val == 0xA || val == 0xC || val == 0xE || val == 0x10) {
                            Function fn = currentProgram.getFunctionManager()
                                    .getFunctionContaining(instr.getAddress());
                            if (fn != null) {
                                long fva = fn.getEntryPoint().getOffset();
                                fnOffsets.computeIfAbsent(fva, k -> new HashSet<>()).add((int)val);
                            }
                        }
                    }
                }
            }
        }

        // Keep functions accessing at least +0xA and +0xC (DLG x and y)
        Set<Long> result = new LinkedHashSet<>();
        for (Map.Entry<Long, Set<Integer>> e : fnOffsets.entrySet()) {
            Set<Integer> offsets = e.getValue();
            if (offsets.contains(0xA) && offsets.contains(0xC)) {
                result.add(e.getKey());
            }
        }
        return result;
    }

    private void dumpAt(long va) throws Exception {
        if (dumped.contains(va)) {
            out.println("// (already dumped) 0x" + Long.toHexString(va));
            return;
        }
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
