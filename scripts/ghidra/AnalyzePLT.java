// Targets: PILOT struct gaps 1 (0xB0-0xC1), 3 (0x2018-0x20B7), 4 (0x21F8-0x25DF)
// via PilotSave, stats-flush, and fort-stats functions.
// Gap 2 (0xCF-0x5AE) is variable-length text -- use differential saves instead.

public class AnalyzePLT extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzePLT");
        // Gap 1 (0xB0-0xC1): pilot card display cluster
        header("PLT -- PilotSave (0x467180) [forced]");
        dumpAtForced(0x00467180L);
        header("PLT -- FUN_004674f0 pilot card display [forced]");
        dumpAtForced(0x004674f0L);
        header("PLT -- pilot screen cluster 0x467000-0x467FFF");
        dumpRange(0x00467000L, 0x00467fffL);
        header("PLT -- callers of PilotSave");
        dumpCallers(0x00467180L);

        // Gap 3 (0x2018-0x20B7): end-of-mission stats flush
        header("PLT -- _EndOfMissionStats@0 (0x484D90) [forced]");
        dumpAtForced(0x00484d90L);
        header("PLT -- FUN_00485380 stats flush (0x485380) [forced]");
        dumpAtForced(0x00485380L);
        header("PLT -- _WpnStats@28 (0x4854E0) [forced]");
        dumpAtForced(0x004854e0L);
        header("PLT -- FUN_004856f0 weapon accuracy dispatch [forced]");
        dumpAtForced(0x004856f0L);
        header("PLT -- FUN_004854a0 weapon accuracy accumulator [forced]");
        dumpAtForced(0x004854a0L);
        header("PLT -- _KillStats@12 (0x485820) [forced]");
        dumpAtForced(0x00485820L);
        header("PLT -- _LandingStats@12 (0x485A40) [forced]");
        dumpAtForced(0x00485a40L);
        header("PLT -- callers of FUN_00485380");
        dumpCallers(0x00485380L);
        header("PLT -- callers of _EndOfMissionStats@0");
        dumpCallers(0x00484d90L);

        // Gap 4 (0x21F8-0x25DF): fort/campaign-phase stats
        header("PLT -- _EndOfFortMissionStats@0 (0x485040) [forced]");
        dumpAtForced(0x00485040L);
        header("PLT -- callers of _EndOfFortMissionStats@0");
        dumpCallers(0x00485040L);
        header("PLT -- stats cluster 0x484000-0x486FFF");
        dumpRange(0x00484000L, 0x00486fffL);

        // Struct scan: all globals in the four gap VA ranges
        header("PLT -- CampaignSave (0x481320) [forced]");
        dumpAtForced(0x00481320L);
        header("PLT -- campaign cluster 0x480000-0x484FFF");
        dumpRange(0x00480000L, 0x00484fffL);

        // Offset scan for gap VA windows (arg 1/2 = function search range; arg 3/4 = struct offset range)
        header("PLT -- global refs in gap-1 range (0xB0-0xC1)");
        for (long va : findFunctionsReadingOffsets(0x00400000L, 0x00510000L, 0xB0, 0xC1)) dumpAt(va);
        header("PLT -- global refs in gap-3 range (0x2018-0x20B7)");
        for (long va : findFunctionsReadingOffsets(0x00400000L, 0x00510000L, 0x2018, 0x20B7)) dumpAt(va);
        header("PLT -- global refs in gap-4 range (0x21F8-0x25DF)");
        for (long va : findFunctionsReadingOffsets(0x00400000L, 0x00510000L, 0x21F8, 0x25DF)) dumpAt(va);

        header("PLT -- symbols matching pilot/campaign/save/stats/score/rank/fort");
        dumpSymbolsMatching("pilot", "pilotsave", "pilotload", "campaign",
                "campaignsave", "campaignload", "stats", "score", "rank", "fort",
                "fortmission", "killstats", "landingstats", "wpnstats", "endofmission");
        closeOutput();
    }
}
