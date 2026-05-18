// Consolidates: DumpHGRFormat, DumpHGRLoader, DumpHGRT2Bit14

public class AnalyzeHGR extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeHGR");

        // HGR loader
        header("FUN_004543c0 (HGR loader)");
        dumpAt(0x004543c0L);

        header("Callers of FUN_004543c0");
        dumpCallers(0x004543c0L);

        header("FUN_004809d0");
        dumpAt(0x004809d0L);

        header("FUN_00480150");
        dumpAt(0x00480150L);

        header("FUN_004801a0");
        dumpAt(0x004801a0L);

        header("HGR/hangar range 0x480000-0x480600");
        dumpRange(0x00480000L, 0x00480600L);

        // HUD draw range (hangar cockpit overlap)
        header("HUD/hangar draw range 0x406000-0x406200");
        dumpRange(0x00406000L, 0x00406200L);

        // Key globals
        header("Xrefs to DAT_004fbbf0");
        dumpXrefsToData(0x004fbbf0L);

        header("Xrefs to hangarName (0x4fb1e8)");
        dumpXrefsToData(0x004fb1e8L);

        // T2 / tile functions referenced from HGR
        header("_G_TileInit (0x447a40)");
        dumpAt(0x00447a40L);

        header("@G_Tile@32 (0x447aa5)");
        dumpAt(0x00447aa5L);

        header("_T_Init2 (0x4c5f60)");
        dumpAt(0x004c5f60L);

        header("tileExpand (0x4f4f78)");
        dumpAt(0x004f4f78L);

        header("_expandTerrain (0x50e145)");
        dumpAt(0x0050e145L);

        // HUD bit 14 in context of HGR (full scan)
        header("Bit 14 (0x4000) scan 0x400000-0x550000");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x4000L)) dumpAt(va);

        // Symbol search
        header("Symbols matching hgr/hangar/airbase/h_airb/t2/terrain/tile/tileexpand");
        dumpSymbolsMatching("hgr", "hangar", "airbase", "h_airb", "t2", "terrain", "tile",
                "tileexpand", "expandterrain");

        closeOutput();
    }

    private void dumpXrefsToData(long va) throws Exception {
        ghidra.program.model.address.Address addr = toAddr(va);
        for (ghidra.program.model.symbol.Reference ref :
                currentProgram.getReferenceManager().getReferencesTo(addr)) {
            ghidra.program.model.listing.Function fn =
                    currentProgram.getFunctionManager().getFunctionContaining(ref.getFromAddress());
            if (fn != null) dumpAt(fn.getEntryPoint().getOffset());
        }
    }
}
