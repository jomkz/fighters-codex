// Consolidates: DumpLAYFunctions, DumpLAYFunctions2, DumpLAYFunctions3,
//               DumpLAYFunctions4, DumpLAYGaps, DumpLAYGradient,
//               DumpLAYRemaining, DumpLAYStructure

public class AnalyzeLAY extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeLAY");
        header("LAY -- ParseLayerFile (0x4b4370)");
        dumpAt(0x004b4370L);
        header("LAY -- CopyLayersToRuntime (0x4b3750)");
        dumpAt(0x004b3750L);
        header("LAY -- InterpolateLayers (0x4b3820)");
        dumpAt(0x004b3820L);
        header("LAY -- GetLayerAtAltitude (0x4b3be0)");
        dumpAt(0x004b3be0L);
        header("LAY -- T_DefaultHorizon (0x4aacf0)");
        dumpAt(0x004aacf0L);
        header("LAY -- ApplyBrightnessGradient (0x4b3cb0)");
        dumpAt(0x004b3cb0L);
        header("LAY -- UpdateSkyState (0x4b3d90)");
        dumpAt(0x004b3d90L);
        header("LAY -- UpdateAuroraClouds (0x4b4170)");
        dumpAt(0x004b4170L);
        header("LAY -- FindNearestColorEntry (0x4b3ad0)");
        dumpAt(0x004b3ad0L);
        header("LAY -- LoadPICByWildcard (0x4b4680)");
        dumpAt(0x004b4680L);
        header("LAY -- GetLayerBoundary (0x4b3190)");
        dumpAt(0x004b3190L);
        header("LAY -- GetLayerByIndex (0x4b3170)");
        dumpAt(0x004b3170L);
        header("LAY -- WRFogLayerUpdate (0x4b4320)");
        dumpAt(0x004b4320L);
        header("LAY -- FUN_004b4790");
        dumpAt(0x004b4790L);
        header("LAY -- hdrPtr writer 1 (0x4b46d0)");
        dumpAt(0x004b46d0L);
        header("LAY -- interpolation cluster 0x4b3b60-0x4b3d90");
        dumpAt(0x004b3b60L); dumpAt(0x004b3b80L); dumpAt(0x004b4680L);
        dumpAt(0x004b46f0L); dumpAt(0x004b4700L); dumpAt(0x004b4720L);
        header("LAY -- callers of ParseLayerFile");
        dumpCallers(0x004b4370L);
        header("LAY -- callers of FindNearestColorEntry");
        dumpCallers(0x004b3ad0L);
        header("LAY -- callers of WRFogLayerUpdate");
        dumpCallers(0x004b4320L);
        header("LAY -- callers of FUN_004b3410");
        dumpCallers(0x004b3410L);
        header("LAY -- xrefs to hdrPtr (DAT_00580d94)");
        dumpXrefsToData(0x00580d94L);
        header("LAY -- xrefs to curLayers (DAT_00583250)");
        dumpXrefsToData(0x00583250L);
        header("LAY -- xrefs to DAT_00583da8");
        dumpXrefsToData(0x00583da8L);
        header("LAY -- xrefs to gap globals 0x580dc4-0x580e18");
        for (long g = 0x00580dc4L; g <= 0x00580e18L; g += 4) dumpXrefsToData(g);
        header("LAY -- all functions 0x4b2ea0-0x4b4200");
        dumpRange(0x004b2ea0L, 0x004b4200L);
        header("LAY -- symbols matching cloud/layer/horizon/fog/weather/gradient");
        dumpSymbolsMatching("cloud", "layer", "horizon", "fog", "weather", "gradient",
                "lay", "aurora", "sky", "haze");
        closeOutput();
    }
}
