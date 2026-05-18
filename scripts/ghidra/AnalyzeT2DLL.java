// T2 terrain overlay DLL analysis.
// The T2 binary format (BIT2 magic) is loaded by an overlay DLL whose location
// in FA.EXE has not yet been pinned down. This script searches for it via:
//   1. The BIT2 magic bytes string search
//   2. A range scan around do_use_terrain_detail (0x4d2344)
//   3. A range scan around tileExpand (0x4f4f78) and expandTerrain (0x50e145)
//   4. An offset scan for T2 sub-header constants
//   5. A surface-class constant scan looking for byte→PIC atlas mapping
//
// @category FightersAnthology
// @author fighters-toolkit

public class AnalyzeT2DLL extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeT2DLL");

        // Known T2 entry points (from AnalyzeT2.java)
        header("do_use_terrain_detail (0x4d2344)");
        dumpAt(0x004d2344L);

        header("Callers of do_use_terrain_detail");
        dumpCallers(0x004d2344L);

        header("@G_Tile@32 (0x447aa5)");
        dumpAt(0x00447aa5L);

        header("tileExpand (0x4f4f78)");
        dumpAt(0x004f4f78L);

        header("Callers of tileExpand");
        dumpCallers(0x004f4f78L);

        header("expandTerrain (0x50e145)");
        dumpAt(0x0050e145L);

        header("Callers of expandTerrain");
        dumpCallers(0x0050e145L);

        // T2 DLL loader area — likely near the terrain/tile subsystem
        header("T2 DLL load area 0x4d2000-0x4d5000");
        dumpRange(0x004d2000L, 0x004d5000L);

        header("T2 / tileExpand area 0x4f4000-0x4f6000");
        dumpRange(0x004f4000L, 0x004f6000L);

        header("expandTerrain area 0x50e000-0x510000");
        dumpRange(0x0050e000L, 0x00510000L);

        // T2 sub-header constants (confirmed: 0x95=149, 0x80=128, 0xC3=195, 0x15=21)
        // Look for functions comparing against these to find the T2 binary parser
        header("T2 sub-header constant 0x95 (149) scan 0x4d0000-0x510000");
        for (long va : findFunctionsWithMask(0x004d0000L, 0x00510000L, 0x95L)) dumpAt(va);

        header("T2 sub-header constant 0xC3 (195) scan");
        for (long va : findFunctionsWithMask(0x004d0000L, 0x00510000L, 0xC3L)) dumpAt(va);

        // Surface-class constants — byte values 0x00-0x1F are typical tile class IDs
        // Search for functions reading byte offsets in that range that also touch PIC loaders
        header("Surface-class offset scan (byte 0x00-0x20) in T2 area");
        for (long va : findFunctionsReadingOffsets(0x004d0000L, 0x00510000L, 0, 0x20)) dumpAt(va);

        // BIT2 magic and .T2 extension string search
        header("String search: BIT2 / T2 terrain keywords");
        searchStrings(new String[]{"BIT2", ".T2", ".t2", "tmap", "tdic",
                "terrain", "TERRAIN", "surface", "tiletype", "tileclass"});

        // GetGround — ground elevation query, uses T2 data
        header("_GetGround@0 (0x47af70)");
        dumpAt(0x0047af70L);

        header("Callers of _GetGround@0");
        dumpCallers(0x0047af70L);

        // MAPWorldToScreen — coordinate transform, references T2 tile grid
        header("?MAPWorldToScreen (0x422380)");
        dumpAt(0x00422380L);

        // Symbol search
        header("Symbols matching t2/terrain/tile/surface/ground/bit2/expand");
        dumpSymbolsMatching("t2", "terrain", "tile", "surface", "ground",
                "bit2", "expand", "tilemap", "tileclass", "terrainload",
                "t2init", "t2load", "t2parse");

        closeOutput();
    }
}
