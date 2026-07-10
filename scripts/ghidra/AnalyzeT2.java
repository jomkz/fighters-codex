// Consolidates: DumpT2Loader, DumpT2MMCoords

public class AnalyzeT2 extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeT2");
        header("T2 -- @G_Tile@32 (0x447aa5)");
        dumpAt(0x00447aa5L);
        header("T2 -- _G_TileInit (0x447a40)");
        dumpAt(0x00447a40L);
        header("T2 -- do_use_terrain_detail (0x4d2344)");
        dumpAt(0x004d2344L);
        header("T2 -- callers of @G_Tile@32");
        dumpCallers(0x00447aa5L);
        header("T2 -- tile cluster 0x447a00-0x447f00");
        dumpRange(0x00447a00L, 0x00447f00L);
        header("T2 -- ?MAPWorldToScreen (0x422380)");
        dumpAt(0x00422380L);
        header("T2 -- _GetGround@0 (0x47af70)");
        dumpAt(0x0047af70L);
        header("T2 -- MM/lib area 0x479e00-0x47a600");
        dumpRange(0x00479e00L, 0x0047a600L);
        header("T2 -- T2 sub-header constant scan (0x95, 0x80, 195, 21)");
        for (long c : new long[]{0x95L, 0x80L, 195L, 21L})
            for (long va : findFunctionsReadingOffsets(0x00400000L, 0x00600000L, (int)c, (int)c))
                dumpAt(va);
        header("T2 -- string search: BIT2 magic / .T2 / tmap / tdic");
        searchStrings(new String[]{"BIT2", ".T2", "tmap", "tdic", "textFormat"});
        header("T2 -- symbols matching t2/terrain/tile/tmap/tdic/mapworld");
        dumpSymbolsMatching("t2", "terrain", "tile", "tmap", "tdic", "mapworld",
                "getground", "worldtoscreen");
        header("T2 -- warhead/hit/fuse symbols");
        dumpSymbolsMatching("warhead", "fuse", "arm", "detonate", "explode", "hit", "prox");
        closeOutput();
    }
}
