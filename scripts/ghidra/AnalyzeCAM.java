// Campaign DLL binary layout analysis.
// Traces the campaign launcher, mission slot array, and weapon/loadout tables.
// Addresses from FA.SMS / prior RE; campaign binary layout is not yet documented.

public class AnalyzeCAM extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeCAM");
        header("CAM -- campaign launcher FUN_00428412");
        dumpAtForced(0x00428412L);
        header("CAM -- callers of campaign launcher");
        dumpCallers(0x00428412L);
        header("CAM -- MC loader (0x481940)");
        dumpAt(0x00481940L);
        header("CAM -- callers of MC loader");
        dumpCallers(0x00481940L);
        header("CAM -- _MISSIONInit2 (0x480b50)");
        dumpAt(0x00480b50L);
        header("CAM -- _ChooseActivity (0x4a08a0)");
        dumpAt(0x004a08a0L);
        header("CAM -- callers of _ChooseActivity");
        dumpCallers(0x004a08a0L);
        header("CAM -- campaign range 0x428000-0x430000");
        dumpRange(0x00428000L, 0x00430000L);
        header("CAM -- asset loader area 0x4a6cc0-0x4a7200");
        dumpRange(0x004a6cc0L, 0x004a7200L);
        header("CAM -- string search: campaign / theater keywords");
        searchStrings(new String[]{".CAM", ".cam", "BALTIC", "PERSIAN", "KOREA",
                "CHINA", "EGYPT", "VIETNAM", "campaign", "theater", "MISSION", "mission"});
        header("CAM -- symbols matching cam/campaign/theater/mission/loadout/mc");
        dumpSymbolsMatching("cam", "campaign", "theater", "mission", "loadout",
                "mcload", "mcproc", "mceval", "choosecampaign", "choosemission");
        closeOutput();
    }
}
