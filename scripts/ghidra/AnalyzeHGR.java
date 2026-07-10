// Consolidates: DumpHGRFormat, DumpHGRLoader, DumpHGRT2Bit14

public class AnalyzeHGR extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeHGR");
        header("HGR -- FUN_004543c0 (loader)");
        dumpAt(0x004543c0L);
        header("HGR -- callers of loader");
        dumpCallers(0x004543c0L);
        header("HGR -- FUN_004809d0");
        dumpAt(0x004809d0L);
        header("HGR -- FUN_00480150");
        dumpAt(0x00480150L);
        header("HGR -- FUN_004801a0");
        dumpAt(0x004801a0L);
        header("HGR -- range 0x480000-0x480600");
        dumpRange(0x00480000L, 0x00480600L);
        header("HGR -- HUD/hangar draw range 0x406000-0x406200");
        dumpRange(0x00406000L, 0x00406200L);
        header("HGR -- xrefs to DAT_004fbbf0");
        dumpXrefsToData(0x004fbbf0L);
        header("HGR -- xrefs to hangarName (0x4fb1e8)");
        dumpXrefsToData(0x004fb1e8L);
        header("HGR -- _G_TileInit (0x447a40)");
        dumpAt(0x00447a40L);
        header("HGR -- @G_Tile@32 (0x447aa5)");
        dumpAt(0x00447aa5L);
        header("HGR -- _T_Init2 (0x4c5f60)");
        dumpAt(0x004c5f60L);
        header("HGR -- tileExpand (0x4f4f78)");
        dumpAt(0x004f4f78L);
        header("HGR -- _expandTerrain (0x50e145)");
        dumpAt(0x0050e145L);
        header("HGR -- bit 14 (0x4000) scan 0x400000-0x550000");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x4000L)) dumpAt(va);
        header("HGR -- symbols matching hgr/hangar/airbase/h_airb/t2/terrain/tile/tileexpand");
        dumpSymbolsMatching("hgr", "hangar", "airbase", "h_airb", "t2", "terrain", "tile",
                "tileexpand", "expandterrain");
        closeOutput();
    }
}
