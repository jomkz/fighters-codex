// Campaign DLL binary layout analysis.
// Traces the campaign launcher, mission slot array, and weapon/loadout tables.
// Addresses from FA.SMS / prior RE; campaign binary layout is not yet documented.
//
// @category FightersAnthology
// @author fighters-toolkit

public class AnalyzeCAM extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeCAM");

        // Campaign launcher entry point
        header("Campaign launcher FUN_00428412 (0x428412)");
        dumpAtForced(0x00428412L);

        header("Callers of campaign launcher");
        dumpCallers(0x00428412L);

        // MC loader — reads .MC (campaign condition DLL) into FA.EXE address space
        header("MC loader (0x481940)");
        dumpAt(0x00481940L);

        header("Callers of MC loader");
        dumpCallers(0x00481940L);

        // MISSIONInit2 — initialises mission state; campaign feeds into it
        header("_MISSIONInit2 (0x480b50)");
        dumpAt(0x00480b50L);

        // ChooseActivity — campaign/mission selection dialog backend
        header("_ChooseActivity (0x4a08a0)");
        dumpAt(0x004a08a0L);

        header("Callers of _ChooseActivity");
        dumpCallers(0x004a08a0L);

        // Campaign / theater name string search
        header("String search: campaign / theater keywords");
        searchStrings(new String[]{".CAM", ".cam", "BALTIC", "PERSIAN", "KOREA",
                "CHINA", "EGYPT", "VIETNAM", "campaign", "theater", "MISSION", "mission"});

        // Range scan around campaign launcher — likely contains mission slot table,
        // aircraft loadout array, and campaign state struct
        header("Campaign range 0x428000-0x430000");
        dumpRange(0x00428000L, 0x00430000L);

        // LoadObject / asset loader — campaign DLLs load their own resources
        header("Asset loader area 0x4a6cc0-0x4a7200");
        dumpRange(0x004a6cc0L, 0x004a7200L);

        // Symbol search
        header("Symbols matching cam/campaign/theater/mission/loadout/mc");
        dumpSymbolsMatching("cam", "campaign", "theater", "mission", "loadout",
                "mcload", "mcproc", "mceval", "choosecampaign", "choosemission");

        closeOutput();
    }
}
