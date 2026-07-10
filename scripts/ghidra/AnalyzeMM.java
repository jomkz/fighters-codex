// Consolidates: DumpMCCAMLoader, DumpMMCAMMission, DumpMMLayerSlot,
//               DumpMMLayerSlot2, DumpMMLayerSlot3, DumpMMLayerSlot4

public class AnalyzeMM extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeMM");
        header("MM -- keyword handler FUN_0047a510");
        dumpAt(0x0047a510L);
        header("MM -- callers of FUN_0047a510");
        dumpCallers(0x0047a510L);
        header("MM -- FUN_0047a130 (MM line parser)");
        dumpAt(0x0047a130L);
        header("MM -- callers of FUN_0047a130");
        dumpCallers(0x0047a130L);
        header("MM -- _MISSIONInit2 variant 1 (0x480b50)");
        dumpAt(0x00480b50L);
        header("MM -- _MISSIONInit2 variant 2 (0x480a30)");
        dumpAt(0x00480a30L);
        header("MM -- callers of _MISSIONInit2");
        dumpCallers(0x00480b50L);
        header("MM -- MC loader (0x481940)");
        dumpAt(0x00481940L);
        header("MM -- callers of MC loader");
        dumpCallers(0x00481940L);
        header("MM -- Pre-MC (0x480750)");
        dumpAt(0x00480750L);
        header("MM -- campaign launcher (0x428412)");
        dumpAt(0x00428412L);
        header("MM -- FUN_00481c10");
        dumpAt(0x00481c10L);
        header("MM -- ParseLayerFile (0x4b4370)");
        dumpAt(0x004b4370L);
        header("MM -- FUN_004b3480");
        dumpAt(0x004b3480L);
        header("MM -- GetLayerByIndex (0x4b3170)");
        dumpAt(0x004b3170L);
        header("MM -- callers of GetLayerByIndex");
        dumpCallers(0x004b3170L);
        header("MM -- FUN_0044f180 / FUN_00430a90 / FUN_0043a5c0");
        dumpAt(0x0044f180L); dumpAt(0x00430a90L); dumpAt(0x0043a5c0L);
        header("MM -- keyword helpers 0x4ace50 / 0x4acfa0");
        dumpAt(0x004ace50L); dumpAt(0x004acfa0L);
        header("MM -- all functions 0x481800-0x482200");
        dumpRange(0x00481800L, 0x00482200L);
        header("MM -- strings: MM keywords and file extensions");
        searchStrings(new String[]{"textFormat", "map", ".T2", ".LAY", "LAY ", "tmap", "tdic",
                "mission", ".MM", "theater"});
        header("MM -- symbols matching mm/map/mission/campaign/cam/mc/theater");
        dumpSymbolsMatching("mm", "map", "mission", "campaign", "cam", "theater",
                "mcload", "mapload", "mapinit");
        closeOutput();
    }
}
