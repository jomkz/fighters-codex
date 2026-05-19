// AI/BI opcode 0x28 (FRAME) consumer analysis.
//
// The FRAME opcode (case 0x28 in the BI bytecode dispatch switch) writes two s16
// values into the CT interpreter state block:
//   DAT_00546c44  (+0x7c from block base DAT_00546bc8)
//   DAT_00546c46  (+0x7e from block base DAT_00546bc8)
// A saved copy is stored at *(DAT_0050cf90) + 0x7c.
//
// The reader is not visible in DumpAllFunctions.txt because it accesses these
// fields through a pointer to the surrounding 128-byte interpreter state block,
// not by direct address.  This script finds the consumer by:
//   1. findFunctionsReadingOffsets(0x400000, 0x540000, 0x7c, 0x80) -- any fn
//      that loads [ptr+0x7c] or [ptr+0x7e] from a struct pointer
//   2. dumpAtForced on the six named candidate functions
//
// Output: %FA_PROJECT%\output\AnalyzeBIFRAME.txt
//
// @category FightersAnthology
// @author fighters-toolkit

public class AnalyzeBIFRAME extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeBIFRAME");

        // ------------------------------------------------------------------
        // 1. FRAME writer -- case 0x28 in the BI dispatch switch
        // ------------------------------------------------------------------
        header("BI opcode 0x28 (FRAME) writer site");
        // The BI dispatch switch lives around 0x4ace00; dump context
        dumpRange(0x004ace00L, 0x004ad800L);

        // ------------------------------------------------------------------
        // 2. CT state block save/restore helpers (confirm block layout)
        // ------------------------------------------------------------------
        header("CT state save FUN_004668f0 (copies 32 dwords from DAT_00546bc8)");
        dumpAt(0x004668f0L);

        header("CT state restore FUN_00466920");
        dumpAt(0x00466920L);

        // ------------------------------------------------------------------
        // 3. findFunctionsReadingOffsets: any fn touching [ptr+0x7c/0x7e]
        // ------------------------------------------------------------------
        header("Functions reading struct offset +0x7c in 0x400000-0x540000 (FRAME s16 consumer candidates)");
        for (long va : findFunctionsReadingOffsets(0x00400000L, 0x00540000L, 0x7c, 0x80)) {
            dumpAt(va);
        }

        // ------------------------------------------------------------------
        // 4. Named candidate consumers (from TODO / prior analysis)
        // ------------------------------------------------------------------
        header("_INFO2Draw (candidate)");
        dumpSymbolsMatching("INFO2Draw", "info2draw", "Info2Draw");

        header("_FMFlight@0 (0x47b020) -- flight model tick, may read FRAME count");
        dumpAtForced(0x0047b020L);

        header("_MANAdd@24 (0x47ceb0) -- manual flight input, may consume FRAME timing");
        dumpAtForced(0x0047ceb0L);

        header("_GVDoCurrentWaypoint -- ground vehicle waypoint tick");
        dumpSymbolsMatching("GVDoCurrentWaypoint", "gvdocurrentwaypoint");

        header("?MPStatusSet@@YIXJ@Z -- multiplayer status update");
        dumpSymbolsMatching("MPStatusSet", "mpstatusset");

        header("FUN_0048e740 (candidate reader near BI interpreter)");
        dumpAtForced(0x0048e740L);

        // ------------------------------------------------------------------
        // 5. Direct-address accesses to DAT_00546c44 / DAT_00546c46 if any
        //    (unlikely after DumpAllFunctions showed none, but double-check)
        // ------------------------------------------------------------------
        header("Callers of DAT_00546c44 / DAT_00546c46 area (direct address accesses)");
        dumpCallers(0x00546c44L);
        dumpCallers(0x00546c46L);

        // ------------------------------------------------------------------
        // 6. Saved-copy pointer -- *(DAT_0050cf90) + 0x7c
        // ------------------------------------------------------------------
        header("DAT_0050cf90 (pointer to saved CT block copy)");
        dumpCallers(0x0050cf90L);

        closeOutput();
    }
}
