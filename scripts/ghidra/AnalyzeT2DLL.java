// T2 terrain overlay DLL analysis.
// The T2 binary format (BIT2 magic) is loaded by an overlay DLL whose location
// in FA.EXE has not yet been pinned down. This script searches for it via:
//   1. The BIT2 magic bytes string search
//   2. Symbol matches for terrain/surface/tile/atlas/class

public class AnalyzeT2DLL extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeT2DLL");
        header("T2DLL -- do_use_terrain_detail (0x4d2344)");
        dumpAt(0x004d2344L);
        header("T2DLL -- callers of do_use_terrain_detail");
        dumpCallers(0x004d2344L);
        header("T2DLL -- @G_Tile@32 (0x447aa5)");
        dumpAt(0x00447aa5L);
        header("T2DLL -- tileExpand (0x4f4f78)");
        dumpAt(0x004f4f78L);
        header("T2DLL -- callers of tileExpand");
        dumpCallers(0x004f4f78L);
        header("T2DLL -- expandTerrain (0x50e145)");
        dumpAt(0x0050e145L);
        header("T2DLL -- callers of expandTerrain");
        dumpCallers(0x0050e145L);
        header("T2DLL -- T2 DLL load area 0x4d2000-0x4d5000");
        dumpRange(0x004d2000L, 0x004d5000L);
        header("T2DLL -- tileExpand area 0x4f4000-0x4f6000");
        dumpRange(0x004f4000L, 0x004f6000L);
        header("T2DLL -- expandTerrain area 0x50e000-0x510000");
        dumpRange(0x0050e000L, 0x00510000L);
        header("T2DLL -- sub-header constant 0x95 scan 0x4d0000-0x510000");
        for (long va : findFunctionsWithMask(0x004d0000L, 0x00510000L, 0x95L)) dumpAt(va);
        header("T2DLL -- sub-header constant 0xC3 scan");
        for (long va : findFunctionsWithMask(0x004d0000L, 0x00510000L, 0xC3L)) dumpAt(va);
        header("T2DLL -- surface-class offset scan (0-0x20) in T2 area");
        for (long va : findFunctionsReadingOffsets(0x004d0000L, 0x00510000L, 0, 0x20)) dumpAt(va);
        header("T2DLL -- string search: BIT2 / T2 terrain keywords");
        searchStrings(new String[]{"BIT2", ".T2", ".t2", "tmap", "tdic",
                "terrain", "TERRAIN", "surface", "tiletype", "tileclass"});
        header("T2DLL -- _GetGround@0 (0x47af70)");
        dumpAt(0x0047af70L);
        header("T2DLL -- callers of _GetGround@0");
        dumpCallers(0x0047af70L);
        header("T2DLL -- ?MAPWorldToScreen (0x422380)");
        dumpAt(0x00422380L);
        header("T2DLL -- symbols matching t2/terrain/tile/surface/ground/bit2/expand");
        dumpSymbolsMatching("t2", "terrain", "tile", "surface", "ground",
                "bit2", "expand", "tilemap", "tileclass", "terrainload",
                "t2init", "t2load", "t2parse");
        closeOutput();
    }
}
