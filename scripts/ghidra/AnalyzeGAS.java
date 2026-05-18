// Consolidates: DumpBRFFunctions, DumpGASFuel, DumpGVProcGAS,
//               DumpGVProcHandlers

public class AnalyzeGAS extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeGAS");

        // GVProc â€” vehicle AI processor
        header("_GVProc (0x473db0)");
        dumpAt(0x00473db0L);

        header("Callers of _GVProc");
        dumpCallers(0x00473db0L);

        header("FUN_00473f50");
        dumpAt(0x00473f50L);

        header("FUN_00473be0");
        dumpAt(0x00473be0L);

        header("LAB_004736f0");
        dumpAt(0x004736f0L);

        // Hardpoint / weapon access
        header("_HARDPtrs@12 (0x452770)");
        dumpAt(0x00452770L);

        header("?HARDPtrsFort (0x452870)");
        dumpAt(0x00452870L);

        header("_HARDFindProj@16 (0x452ff0)");
        dumpAt(0x00452ff0L);

        header("Callers of _HARDFindProj@16");
        dumpCallers(0x00452ff0L);

        header("@HardpointAngle@4 (0x4ab7f0)");
        dumpAt(0x004ab7f0L);

        // Fuel system
        header("@FMFuelConsumption (0x451e50)");
        dumpAt(0x00451e50L);

        header("_BurnFuel (0x451e80)");
        dumpAt(0x00451e80L);

        header("Callers of _BurnFuel");
        dumpCallers(0x00451e80L);

        header("@FMBurnNPCFuel (0x452050)");
        dumpAt(0x00452050L);

        header("_HARDTotalFuel (0x453a70)");
        dumpAt(0x00453a70L);

        header("?MPSetFuel (0x4723a0)");
        dumpAt(0x004723a0L);

        // BRF / seeker cross-reference
        header("@HARDFindECMForObj (0x452f10)");
        dumpAt(0x00452f10L);

        header("@HARDFindJammer (0x452ea0)");
        dumpAt(0x00452ea0L);

        header("@HARDBestSeeker (0x452e60)");
        dumpAt(0x00452e60L);

        header("@HARDBestSeekers (0x452d90)");
        dumpAt(0x00452d90L);

        header("_PROJLock@24 (0x4c2f20)");
        dumpAt(0x004c2f20L);

        header("_PROJLockUpdate@0 (0x4c0960)");
        dumpAt(0x004c0960L);

        header("_Seek (0x4ad090)");
        dumpAt(0x004ad090L);

        header("_SeekTell (0x4ad000)");
        dumpAt(0x004ad000L);

        header("_LibSeek (0x47a090)");
        dumpAt(0x0047a090L);

        // JT / damage
        header("_SetupJT (0x4a7230)");
        dumpAt(0x004a7230L);

        header("FUN_004a6eb0");
        dumpAt(0x004a6eb0L);

        header("?PROJDamageProc (0x4c1870)");
        dumpAt(0x004c1870L);

        header("_DAMAGEDoHit (0x40f970)");
        dumpAt(0x0040f970L);

        // SEE lobe cross-reference
        header("Search lobe (0x4c2eb0)");
        dumpAt(0x004c2eb0L);

        header("Track lobe (0x4c31f0)");
        dumpAt(0x004c31f0L);

        header("Seeker cone (0x4c2860)");
        dumpAt(0x004c2860L);

        header("Warhead/fuse cluster (0x4c2b50, 0x4c3360, 0x4c20c0, 0x4c5670, 0x4c3960)");
        dumpAt(0x004c2b50L);
        dumpAt(0x004c3360L);
        dumpAt(0x004c20c0L);
        dumpAt(0x004c5670L);
        dumpAt(0x004c3960L);

        // CTDo_ fire-related
        header("_CTEval_do_ir_launch (0x464e70)");
        dumpAt(0x00464e70L);

        header("_CTEval_do_radar_launch (0x464e60)");
        dumpAt(0x00464e60L);

        // Xrefs to CTDo_move (0x465cc0) â€” find BI move dispatch
        header("Xrefs to CTDo_move (0x465cc0)");
        dumpCallers(0x00465cc0L);

        // JT weapon physics struct — entity offsets 0xF6-0x114 cover turn rate, g-limit,
        // and flight-model parameters that have not yet been scanned.
        header("JT entity offsets 0xF6-0x114 scan in 0x460000-0x490000");
        for (long va : findFunctionsReadingOffsets(0x00460000L, 0x00490000L, 0xF6, 0x114)) dumpAt(va);

        // Widen to full range in case JT physics code lives outside the BI/CTDo cluster
        header("JT entity offsets 0xF6-0x114 wide scan 0x400000-0x510000");
        for (long va : findFunctionsReadingOffsets(0x00400000L, 0x00510000L, 0xF6, 0x114)) dumpAt(va);

        // JT warhead flag bits 1-3 and 5-6 — no function found testing them in prior analysis;
        // may be unused or set only at load time; scan JT setup cluster
        header("JT warhead flag bit 1 (0x2) in 0x4a6000-0x4a8000");
        for (long va : findFunctionsWithMask(0x004a6000L, 0x004a8000L, 0x2L)) dumpAt(va);

        header("JT warhead flag bit 2 (0x4) in 0x4a6000-0x4a8000");
        for (long va : findFunctionsWithMask(0x004a6000L, 0x004a8000L, 0x4L)) dumpAt(va);

        header("JT warhead flag bit 3 (0x8) in 0x4a6000-0x4a8000");
        for (long va : findFunctionsWithMask(0x004a6000L, 0x004a8000L, 0x8L)) dumpAt(va);

        header("JT warhead flag bits 5-6 (0x20/0x40) in 0x4a6000-0x4a8000");
        for (long m : new long[]{0x20L, 0x40L})
            for (long va : findFunctionsWithMask(0x004a6000L, 0x004a8000L, m)) dumpAt(va);

        // Symbol search
        header("Symbols matching gas/fuel/burn/fmfuel/hard/brf/gvproc/spawn/load/tank/turnrate/glimit");
        dumpSymbolsMatching("gas", "fuel", "burn", "fmfuel", "hard", "brf", "gvproc",
                "spawn", "tank", "refuel", "afterburner", "turnrate", "glimit", "maxg",
                "maxspeed", "minspeed", "accel", "decel", "jt_", "setupjt");

        closeOutput();
    }
}
