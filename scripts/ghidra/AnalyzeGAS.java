// Consolidates: DumpBRFFunctions, DumpGASFuel, DumpGVProcGAS,
//               DumpGVProcHandlers

public class AnalyzeGAS extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeGAS");
        header("GAS -- _GVProc (0x473db0)");
        dumpAt(0x00473db0L);
        header("GAS -- callers of _GVProc");
        dumpCallers(0x00473db0L);
        header("GAS -- FUN_00473f50");
        dumpAt(0x00473f50L);
        header("GAS -- FUN_00473be0");
        dumpAt(0x00473be0L);
        header("GAS -- LAB_004736f0");
        dumpAt(0x004736f0L);
        header("GAS -- _HARDPtrs@12 (0x452770)");
        dumpAt(0x00452770L);
        header("GAS -- ?HARDPtrsFort (0x452870)");
        dumpAt(0x00452870L);
        header("GAS -- _HARDFindProj@16 (0x452ff0)");
        dumpAt(0x00452ff0L);
        header("GAS -- callers of _HARDFindProj@16");
        dumpCallers(0x00452ff0L);
        header("GAS -- @HardpointAngle@4 (0x4ab7f0)");
        dumpAt(0x004ab7f0L);
        header("GAS -- @FMFuelConsumption (0x451e50)");
        dumpAt(0x00451e50L);
        header("GAS -- _BurnFuel (0x451e80)");
        dumpAt(0x00451e80L);
        header("GAS -- callers of _BurnFuel");
        dumpCallers(0x00451e80L);
        header("GAS -- @FMBurnNPCFuel (0x452050)");
        dumpAt(0x00452050L);
        header("GAS -- _HARDTotalFuel (0x453a70)");
        dumpAt(0x00453a70L);
        header("GAS -- ?MPSetFuel (0x4723a0)");
        dumpAt(0x004723a0L);
        header("GAS -- @HARDFindECMForObj (0x452f10)");
        dumpAt(0x00452f10L);
        header("GAS -- @HARDFindJammer (0x452ea0)");
        dumpAt(0x00452ea0L);
        header("GAS -- @HARDBestSeeker (0x452e60)");
        dumpAt(0x00452e60L);
        header("GAS -- @HARDBestSeekers (0x452d90)");
        dumpAt(0x00452d90L);
        header("GAS -- _PROJLock@24 (0x4c2f20)");
        dumpAt(0x004c2f20L);
        header("GAS -- _PROJLockUpdate@0 (0x4c0960)");
        dumpAt(0x004c0960L);
        header("GAS -- _Seek (0x4ad090)");
        dumpAt(0x004ad090L);
        header("GAS -- _SeekTell (0x4ad000)");
        dumpAt(0x004ad000L);
        header("GAS -- _LibSeek (0x47a090)");
        dumpAt(0x0047a090L);
        header("GAS -- _SetupJT (0x4a7230)");
        dumpAt(0x004a7230L);
        header("GAS -- FUN_004a6eb0");
        dumpAt(0x004a6eb0L);
        header("GAS -- ?PROJDamageProc (0x4c1870)");
        dumpAt(0x004c1870L);
        header("GAS -- _DAMAGEDoHit (0x40f970)");
        dumpAt(0x0040f970L);
        header("GAS -- SEE lobe cross-reference");
        dumpAt(0x004c2eb0L); dumpAt(0x004c31f0L); dumpAt(0x004c2860L);
        header("GAS -- warhead/fuse cluster");
        dumpAt(0x004c2b50L); dumpAt(0x004c3360L); dumpAt(0x004c20c0L);
        dumpAt(0x004c5670L); dumpAt(0x004c3960L);
        header("GAS -- _CTEval_do_ir_launch (0x464e70)");
        dumpAt(0x00464e70L);
        header("GAS -- _CTEval_do_radar_launch (0x464e60)");
        dumpAt(0x00464e60L);
        header("GAS -- xrefs to CTDo_move (0x465cc0)");
        dumpCallers(0x00465cc0L);
        header("GAS -- JT entity offsets 0xF6-0x114 scan 0x460000-0x490000");
        for (long va : findFunctionsReadingOffsets(0x00460000L, 0x00490000L, 0xF6, 0x114)) dumpAt(va);
        header("GAS -- JT entity offsets 0xF6-0x114 wide scan 0x400000-0x510000");
        for (long va : findFunctionsReadingOffsets(0x00400000L, 0x00510000L, 0xF6, 0x114)) dumpAt(va);
        header("GAS -- JT warhead flag bits 1-3 and 5-6 in 0x4a6000-0x4a8000");
        for (long m : new long[]{0x2L, 0x4L, 0x8L, 0x20L, 0x40L})
            for (long va : findFunctionsWithMask(0x004a6000L, 0x004a8000L, m)) dumpAt(va);
        header("GAS -- symbols matching gas/fuel/burn/fmfuel/hard/brf/gvproc/spawn");
        dumpSymbolsMatching("gas", "fuel", "burn", "fmfuel", "hard", "brf", "gvproc",
                "spawn", "tank", "refuel", "afterburner", "turnrate", "glimit", "maxg",
                "maxspeed", "minspeed", "accel", "decel", "jt_", "setupjt");
        closeOutput();
    }
}
