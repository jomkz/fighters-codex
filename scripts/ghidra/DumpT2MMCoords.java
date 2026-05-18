import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class DumpT2MMCoords extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;
    private Set<Long> dumped = new LinkedHashSet<>();

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\t2_mm_coords.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // Goal 1: T2 class constants — bytes 4-16 of the T2 sub-header contain
        // 3 distinct class constants. _G_TileInit (0x447a40) is the graphics buffer,
        // not the T2 parser. Find the actual T2 reader by tracing @G_Tile@32 (0x447aa5)
        // which renders a tile — it must read the class byte to select texture/palette.
        // Also trace do_use_terrain_detail (0x4d2344) which explicitly tests terrain class.
        out.println("// === @G_Tile@32 (0x447aa5) ===");
        dumpAt(0x00447aa5L);

        out.println("\n// === _G_TileInit (0x447a40) ===");
        dumpAt(0x00447a40L);

        out.println("\n// === do_use_terrain_detail (0x4d2344) ===");
        dumpAt(0x004d2344L);

        // Callers of @G_Tile@32 — find the terrain rendering / T2 iteration loop.
        out.println("\n// === Callers of @G_Tile@32 (0x447aa5) ===");
        Address gTileAddr = toAddr(0x00447aa5L);
        Set<Long> gTileCallers = new LinkedHashSet<>();
        for (Reference ref : currentProgram.getReferenceManager().getReferencesTo(gTileAddr)) {
            if (ref.getReferenceType().isCall()) {
                Function fn = currentProgram.getFunctionManager()
                        .getFunctionContaining(ref.getFromAddress());
                if (fn != null && gTileCallers.add(fn.getEntryPoint().getOffset())) {
                    out.println("// CALLER: " + fn.getName() + " @ " + fn.getEntryPoint());
                }
            }
        }
        for (long va : gTileCallers) dumpAt(va);

        // Functions in the tile cluster (0x447a00-0x447f00).
        out.println("\n// === Functions 0x447a00-0x447f00 (tile cluster) ===");
        for (Function fn : currentProgram.getFunctionManager().getFunctions(true)) {
            long ep = fn.getEntryPoint().getOffset();
            if (ep >= 0x00447a00L && ep <= 0x00447f00L && !dumped.contains(ep)) {
                dumpAt(ep);
            }
        }

        // Goal 2: MM world-space coordinate scale.
        // ?MAPWorldToScreen (0x422380) converts a 3D world-space point to screen coords.
        // The MM file stores pos/view as world-space ints. Dump this function and its
        // callers to understand the coordinate unit and scale factor.
        out.println("\n// === ?MAPWorldToScreen (0x422380) ===");
        dumpAt(0x00422380L);

        // _GetGround@0 (0x47af70) returns ground height at current position — uses T2 or
        // height-map data. Dump it to see how terrain elevation is stored.
        out.println("\n// === _GetGround@0 (0x47af70) ===");
        dumpAt(0x0047af70L);

        // FUN_0047a130 was the MM line parser (reads MM text and parses world coords).
        // Dump surrounding MM functions (0x479e00-0x47a600).
        out.println("\n// === Functions 0x479e00-0x47a600 (MM/lib area) ===");
        for (Function fn : currentProgram.getFunctionManager().getFunctions(true)) {
            long ep = fn.getEntryPoint().getOffset();
            if (ep >= 0x00479e00L && ep <= 0x0047a600L && !dumped.contains(ep)) {
                dumpAt(ep);
            }
        }

        // JT warhead bits 1-3, 5-6: search for functions that use missile+0xa6
        // with masks 0x2, 0x4, 0x8, 0x20, 0x40.
        // Scan all references to the known missile+0xa6 access patterns.
        out.println("\n// === FA.SMS symbols matching warhead/hit/fuse/arm ===");
        for (Symbol sym : currentProgram.getSymbolTable().getAllSymbols(false)) {
            String name = sym.getName().toLowerCase();
            if (name.contains("warhead") || name.contains("fuse") || name.contains("arm")
                    || name.contains("detonate") || name.contains("explode")
                    || name.contains("hit") || name.contains("prox")) {
                out.println("// SYM: " + sym.getName() + " @ " + sym.getAddress());
            }
        }

        out.close();
        decompiler.dispose();
        println("Output: C:\\Temp\\t2_mm_coords.txt");
    }

    private void dumpAt(long va) throws Exception {
        if (dumped.contains(va)) return;
        dumped.add(va);
        Address addr = toAddr(va);
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
