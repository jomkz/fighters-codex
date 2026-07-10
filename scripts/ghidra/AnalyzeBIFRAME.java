// AI/BI opcode 0x28 (FRAME) consumer analysis.
//
// The FRAME opcode (case 0x28 in the BI bytecode dispatch switch) writes two s16
// values into the CT interpreter state block at +0x7c/+0x7e (DAT_00546c44/46).
// The writer is confirmed (FUN_00466a80, DumpAllFunctions.txt line 78118).
// Access is via pointer (*(DAT_0050cf90) + 0x7c) -- invisible to static offset scans.
// This script finds consumers via cross-reference to DAT_00546bc8 (CT state base).

public class AnalyzeBIFRAME extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeBIFRAME");
        header("BIFRAME -- FRAME writer site: dispatch area 0x4ace00-0x4ad800");
        dumpRange(0x004ace00L, 0x004ad800L);
        header("BIFRAME -- CT state save FUN_004668f0");
        dumpAt(0x004668f0L);
        header("BIFRAME -- CT state restore FUN_00466920");
        dumpAt(0x00466920L);
        header("BIFRAME -- findFunctionsReadingOffsets +0x7c (FRAME s16 in CT state block)");
        for (long va : findFunctionsReadingOffsets(0x00400000L, 0x00540000L, 0x7c, 0x80)) dumpAt(va);
        header("BIFRAME -- _INFO2Draw candidate");
        dumpSymbolsMatching("INFO2Draw", "info2draw", "Info2Draw");
        header("BIFRAME -- _FMFlight@0 (0x47b020) candidate consumer");
        dumpAtForced(0x0047b020L);
        header("BIFRAME -- _MANAdd@24 (0x47ceb0) candidate consumer");
        dumpAtForced(0x0047ceb0L);
        header("BIFRAME -- _GVDoCurrentWaypoint / MPStatusSet (symbol search)");
        dumpSymbolsMatching("GVDoCurrentWaypoint", "gvdocurrentwaypoint",
                "MPStatusSet", "mpstatusset");
        header("BIFRAME -- FUN_0048e740 candidate consumer");
        dumpAtForced(0x0048e740L);
        header("BIFRAME -- direct-address callers of DAT_00546c44 / DAT_00546c46");
        dumpCallers(0x00546c44L);
        dumpCallers(0x00546c46L);
        header("BIFRAME -- callers of DAT_0050cf90 (CT block saved-copy pointer)");
        dumpCallers(0x0050cf90L);
        closeOutput();
    }
}
