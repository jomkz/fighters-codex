import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.scalar.*;
import java.io.*;
import java.util.*;

public class DumpHGRT2Bit14 extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;
    private Set<Long> dumped = new LinkedHashSet<>();

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\hgr_t2_bit14.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // === Goal 1: HGR hangar layout ===
        // ?hangarName@@3PADA @ 0x004FB1E8 is the hangar filename string.
        // Find all references to this data address from code.
        out.println("// === ?hangarName@@3PADA data @ 0x004FB1E8 ===");
        Address hangarNameAddr = toAddr(0x004FB1E8L);
        out.println("// References to hangarName:");
        Set<Long> hangarRefs = new LinkedHashSet<>();
        for (Reference ref : currentProgram.getReferenceManager().getReferencesTo(hangarNameAddr)) {
            Function fn = currentProgram.getFunctionManager().getFunctionContaining(ref.getFromAddress());
            if (fn != null && hangarRefs.add(fn.getEntryPoint().getOffset())) {
                out.println("//   from " + fn.getName() + " @ " + fn.getEntryPoint()
                        + " (ref at " + ref.getFromAddress() + ")");
            }
        }
        for (long va : hangarRefs) dumpAt(va);

        // Also scan for "H_AIRB" string reference.
        out.println("\n// === Scanning data for 'H_AIRB' string ===");
        for (Symbol sym : currentProgram.getSymbolTable().getAllSymbols(false)) {
            String name = sym.getName();
            if (name.toUpperCase().contains("H_AIRB") || name.toUpperCase().contains("HANGAR")) {
                out.println("// SYM: " + name + " @ " + sym.getAddress());
                for (Reference ref : currentProgram.getReferenceManager()
                        .getReferencesTo(sym.getAddress())) {
                    Function fn = currentProgram.getFunctionManager()
                            .getFunctionContaining(ref.getFromAddress());
                    if (fn != null && hangarRefs.add(fn.getEntryPoint().getOffset())) {
                        out.println("//   ref from: " + fn.getName() + " @ " + fn.getEntryPoint());
                        dumpAt(fn.getEntryPoint().getOffset());
                    }
                }
            }
        }

        // shellDeBriefWinMusic @ 0x004F3CA8 — debrief screen — may reference hangar init.
        out.println("\n// === Functions near shellDeBriefWinMusic (0x004F3CA8) ===");
        for (Symbol sym : currentProgram.getSymbolTable().getAllSymbols(false)) {
            String name = sym.getName().toLowerCase();
            if (name.contains("debrief") || name.contains("hangar") || name.contains("briefing")) {
                out.println("// SYM: " + sym.getName() + " @ " + sym.getAddress());
                Function fn = currentProgram.getFunctionManager().getFunctionAt(sym.getAddress());
                if (fn != null && hangarRefs.add(fn.getEntryPoint().getOffset())) {
                    dumpAt(fn.getEntryPoint().getOffset());
                }
            }
        }

        // === Goal 2: T2 terrain class constants and surface class ===
        // G_TileInit @ 0x00447A40, @G_Tile@32 @ 0x00447AA5, _T_Init2@0 @ 0x004C5F60
        out.println("\n// === _G_TileInit @ 0x00447A40 ===");
        dumpAt(0x00447A40L);

        out.println("\n// === _G_TileShutDown @ 0x00447A73 ===");
        dumpAt(0x00447A73L);

        out.println("\n// === @G_Tile@32 @ 0x00447AA5 ===");
        dumpAt(0x00447AA5L);

        out.println("\n// === _T_Init2@0 @ 0x004C5F60 ===");
        dumpAt(0x004C5F60L);

        // The T2 sub-header bytes 4-16 contain class constants.
        // Scan for functions reading bytes 4-16 from a tile/terrain pointer.
        // Also look for tileExpand @ 0x004F4F78.
        out.println("\n// === tileExpand data @ 0x004F4F78 ===");
        Address tileExpAddr = toAddr(0x004F4F78L);
        for (Reference ref : currentProgram.getReferenceManager().getReferencesTo(tileExpAddr)) {
            Function fn = currentProgram.getFunctionManager().getFunctionContaining(ref.getFromAddress());
            if (fn != null) {
                out.println("// ref from: " + fn.getName() + " @ " + fn.getEntryPoint());
                dumpAt(fn.getEntryPoint().getOffset());
            }
        }

        // _expandTerrain @ 0x0050E145
        out.println("\n// === _expandTerrain @ 0x0050E145 ===");
        dumpAt(0x0050E145L);

        // Also search for functions using T2 grid-size constants (0x100, 0x200, 0x400 = known grid sizes).
        out.println("\n// === FA.SMS symbols matching T2/tile/terrain ===");
        for (Symbol sym : currentProgram.getSymbolTable().getAllSymbols(false)) {
            String name = sym.getName().toLowerCase();
            if (name.contains("tile") || name.contains("terrain") || name.contains("_t2")
                    || name.contains("t2_") || name.contains("expand")) {
                out.println("// SYM: " + sym.getName() + " @ " + sym.getAddress());
            }
        }

        // === Goal 3: HUD advisory bit 14 (0x4000) writer ===
        // DAT_0050cfef is the HUD state flags word at 0x0050cfef.
        // Scan the entire executable for OR instructions that write 0x4000 to any
        // memory address (the OR [mem], 0x4000 pattern), then cross-check with DAT_0050cfef.
        out.println("\n// === HUD bit 14 (0x4000) writer — full binary scan ===");
        Set<Long> bit14Fns = findFunctionsWithMask(0x00400000L, 0x00550000L, 0x4000L);
        out.println("// Functions using 0x4000 as mask: " + bit14Fns.size());
        for (long va : bit14Fns) {
            out.println("// candidate: " + getFunctionName(va));
        }
        // Dump the most likely candidates (exclude CRT / known non-HUD ranges)
        for (long va : bit14Fns) {
            if (va >= 0x00400000L && va < 0x004F0000L) {
                dumpAt(va);
            }
        }

        out.close();
        decompiler.dispose();
        println("Output: C:\\Temp\\hgr_t2_bit14.txt");
    }

    private String getFunctionName(long va) {
        Address addr = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(va);
        Function fn = currentProgram.getFunctionManager().getFunctionAt(addr);
        return fn != null ? fn.getName() + " @ 0x" + Long.toHexString(va) : "0x" + Long.toHexString(va);
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
            if (!mnem.equals("and") && !mnem.equals("test") && !mnem.equals("or")) continue;
            for (int i = 0; i < instr.getNumOperands(); i++) {
                Object[] objs = instr.getOpObjects(i);
                for (Object obj : objs) {
                    if (obj instanceof Scalar) {
                        long val = ((Scalar) obj).getUnsignedValue();
                        if (val == mask) {
                            Function fn = currentProgram.getFunctionManager()
                                    .getFunctionContaining(instr.getAddress());
                            if (fn != null) {
                                seen.add(fn.getEntryPoint().getOffset());
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
