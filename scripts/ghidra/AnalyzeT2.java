// Consolidates: DumpT2Loader, DumpT2MMCoords

public class AnalyzeT2 extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeT2");

        // T2 tile renderer (known reference point)
        header("@G_Tile@32 (0x447aa5)");
        dumpAt(0x00447aa5L);

        header("_G_TileInit (0x447a40)");
        dumpAt(0x00447a40L);

        header("do_use_terrain_detail (0x4d2344)");
        dumpAt(0x004d2344L);

        // Callers of @G_Tile@32 â€” find the terrain rendering / T2 iteration loop
        header("Callers of @G_Tile@32 (0x447aa5)");
        dumpCallers(0x00447aa5L);

        // Tile cluster
        header("Functions 0x447a00-0x447f00 (tile cluster)");
        dumpRange(0x00447a00L, 0x00447f00L);

        // MM world-space coordinate scale
        header("?MAPWorldToScreen (0x422380)");
        dumpAt(0x00422380L);

        header("_GetGround@0 (0x47af70)");
        dumpAt(0x0047af70L);

        // MM/lib area functions
        header("Functions 0x479e00-0x47a600 (MM/lib area)");
        dumpRange(0x00479e00L, 0x0047a600L);

        // BIT2 magic byte search
        header("'BIT2' magic (0x42 0x49 0x54 0x32) in PE");
        {
            byte[] bit2 = { 0x42, 0x49, 0x54, 0x32 };
            ghidra.program.model.address.AddressSpace ds =
                    currentProgram.getAddressFactory().getDefaultAddressSpace();
            ghidra.program.model.address.Address searchStart = ds.getAddress(0x00400000L);
            ghidra.program.model.address.Address searchEnd   = ds.getAddress(0x00600000L);
            ghidra.program.model.address.Address found = currentProgram.getMemory().findBytes(
                    searchStart, searchEnd, bit2, null, true, monitor);
            int count = 0;
            while (found != null) {
                count++;
                out.println("// 'BIT2' at " + found);
                for (ghidra.program.model.symbol.Reference ref :
                        currentProgram.getReferenceManager().getReferencesTo(found)) {
                    ghidra.program.model.listing.Function f =
                            currentProgram.getFunctionManager().getFunctionContaining(ref.getFromAddress());
                    if (f != null) dumpAt(f.getEntryPoint().getOffset());
                }
                found = currentProgram.getMemory().findBytes(
                        found.add(1), searchEnd, bit2, null, true, monitor);
            }
            out.println("// BIT2 occurrences: " + count);
        }

        // String / keyword search
        header("T2-related keyword strings");
        searchStrings(new String[]{"textFormat", ".T2", "T2", "tmap", "tdic", "BIT2"});

        // T2 sub-header constant scan
        header("Functions using T2 sub-header constants (0x95, 0x80, 195, 21) in 0x400000-0x600000");
        for (long c : new long[]{0x95L, 0x80L, 195L, 21L}) {
            out.println("// --- constant 0x" + Long.toHexString(c) + " ---");
            for (long va : findFunctionsReadingOffsets(0x00400000L, 0x00600000L, (int)c, (int)c))
                dumpAt(va);
        }

        // Warhead/hit/fuse symbols (T2 cross-reference from DumpT2MMCoords)
        header("FA.SMS symbols matching warhead/hit/fuse/arm/detonate/explode/prox");
        dumpSymbolsMatching("warhead", "fuse", "arm", "detonate", "explode", "hit", "prox");

        // Symbol search
        header("Symbols matching t2/terrain/tile/tmap/tdic/mapworld");
        dumpSymbolsMatching("t2", "terrain", "tile", "tmap", "tdic", "mapworld",
                "getground", "worldtoscreen");

        closeOutput();
    }
}
