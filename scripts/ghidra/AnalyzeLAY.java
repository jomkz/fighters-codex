// Consolidates: DumpLAYFunctions, DumpLAYFunctions2, DumpLAYFunctions3,
//               DumpLAYFunctions4, DumpLAYGaps, DumpLAYGradient,
//               DumpLAYRemaining, DumpLAYStructure

public class AnalyzeLAY extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeLAY");

        // Named loader + horizon functions
        header("ParseLayerFile (0x4b4370)");
        dumpAt(0x004b4370L);

        header("CopyLayersToRuntime (0x4b3750)");
        dumpAt(0x004b3750L);

        header("InterpolateLayers (0x4b3820)");
        dumpAt(0x004b3820L);

        header("GetLayerAtAltitude (0x4b3be0)");
        dumpAt(0x004b3be0L);

        header("T_DefaultHorizon (0x4aacf0)");
        dumpAt(0x004aacf0L);

        header("ApplyBrightnessGradient (0x4b3cb0)");
        dumpAt(0x004b3cb0L);

        header("UpdateSkyState (0x4b3d90)");
        dumpAt(0x004b3d90L);

        header("UpdateAuroraClouds (0x4b4170)");
        dumpAt(0x004b4170L);

        header("FindNearestColorEntry (0x4b3ad0)");
        dumpAt(0x004b3ad0L);

        header("LoadPICByWildcard (0x4b4680)");
        dumpAt(0x004b4680L);

        header("GetLayerBoundary (0x4b3190)");
        dumpAt(0x004b3190L);

        header("GetLayerByIndex (0x4b3170)");
        dumpAt(0x004b3170L);

        header("WRFogLayerUpdate (0x4b4320)");
        dumpAt(0x004b4320L);

        header("FUN_004b4790");
        dumpAt(0x004b4790L);

        // hdrPtr writers
        header("hdrPtr writer 1 (0x4b46d0)");
        dumpAt(0x004b46d0L);

        // Interpolation cluster
        header("Interpolation functions 0x4b3b60-0x4b3d90");
        dumpAt(0x004b3b60L);
        dumpAt(0x004b3b80L);
        dumpAt(0x004b4680L);
        dumpAt(0x004b46f0L);
        dumpAt(0x004b4700L);
        dumpAt(0x004b4720L);

        // Callers
        header("Callers of ParseLayerFile (0x4b4370)");
        dumpCallers(0x004b4370L);

        header("Callers of FindNearestColorEntry (0x4b3ad0)");
        dumpCallers(0x004b3ad0L);

        header("Callers of WRFogLayerUpdate (0x4b4320)");
        dumpCallers(0x004b4320L);

        header("Callers of FUN_004b3410");
        dumpCallers(0x004b3410L);

        // Xrefs to key globals
        header("Xrefs to hdrPtr (DAT_00580d94)");
        dumpXrefsToData(0x00580d94L);

        header("Xrefs to curLayers (DAT_00583250)");
        dumpXrefsToData(0x00583250L);

        header("Xrefs to DAT_00583da8");
        dumpXrefsToData(0x00583da8L);

        header("Xrefs to gap globals 0x580dc4-0x580e18");
        for (long g = 0x00580dc4L; g <= 0x00580e18L; g += 4) dumpXrefsToData(g);

        // Full LAY function range
        header("All functions 0x4b2ea0-0x4b4200");
        dumpRange(0x004b2ea0L, 0x004b4200L);

        // Symbol search
        header("Symbols matching cloud/layer/horizon/fog/weather/gradient");
        dumpSymbolsMatching("cloud", "layer", "horizon", "fog", "weather", "gradient",
                "lay", "aurora", "sky", "haze");

        closeOutput();
    }

    private void dumpXrefsToData(long va) throws Exception {
        ghidra.program.model.address.Address addr = toAddr(va);
        boolean any = false;
        for (ghidra.program.model.symbol.Reference ref :
                currentProgram.getReferenceManager().getReferencesTo(addr)) {
            ghidra.program.model.listing.Function fn =
                    currentProgram.getFunctionManager().getFunctionContaining(ref.getFromAddress());
            if (fn != null && !dumped.contains(fn.getEntryPoint().getOffset())) {
                if (!any) {
                    out.println("// refs to " + addr + ":");
                    any = true;
                }
                out.println("//   " + fn.getName() + " @ " + fn.getEntryPoint());
                dumpAt(fn.getEntryPoint().getOffset());
            }
        }
    }
}
