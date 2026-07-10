// Flight model, aerodynamics, and collision detection.
// Dark zone: 0x4D0000-0x4EFFFF (128 KB) -- zero prior coverage.
// Also resolves _PROJProc virtual dispatch and terrain collision path.
// Invoke: run_ghidra.sh AnalyzePhysics.java
// Output: $FA_PROJECT/output/AnalyzePhysics.txt

public class AnalyzePhysics extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzePhysics");
        header("FM -- flight model subsystem symbols");
        dumpSymbolsMatching("_fmupdate", "fmupdate", "_fminit", "fminit",
                "_fmproc", "fmproc", "_fmflight", "fmflight",
                "_lift", "lift", "_drag", "drag", "_thrust", "thrust",
                "_stall", "stall", "_aoa", "aoa", "_airspeed", "airspeed",
                "_mach", "mach", "_altitude", "altitude", "_climb", "climb",
                "_bank", "bank", "_pitch", "pitch", "_roll", "roll",
                "_glimit", "glimit", "_turnrate", "turnrate",
                "@fmflight", "?fmflight", "@fmburnfuel", "fmburnfuel",
                "@fmfuelconsumption", "fmfuelconsumption",
                "@fmburnnpcfuel", "fmburnnpcfuel");
        header("FM -- callers of @FMFuelConsumption (0x451e50)");
        dumpCallers(0x00451e50L);
        header("FM -- callers of _BurnFuel (0x451e80)");
        dumpCallers(0x00451e80L);
        header("PROJ -- vtable scan: callers of _PROJProc wrapper (0x4c1f10)");
        dumpCallers(0x004c1f10L);
        header("PROJ -- _PROJProc dispatch (0x4c1f50)");
        dumpAt(0x004c1f50L);
        header("PROJ -- PROJMoveProc (0x4c11b0)");
        dumpAt(0x004c11b0L);
        header("PROJ -- _PROJSpeed@8 (0x4c1120)");
        dumpAt(0x004c1120L);
        header("PROJ -- _PROJEngineState@0 (0x4c1170)");
        dumpAt(0x004c1170L);
        header("PROJ -- entity field offset scan 0x50-0x7F");
        findFunctionsReadingOffsets(0x004c0000L, 0x004c3000L, 0x50, 0x7F);
        header("PROJ -- entity field offset scan 0xF6-0x11E");
        findFunctionsReadingOffsets(0x00400000L, 0x00540000L, 0xF6, 0x11E);
        header("TERRAIN -- _GetGround@0 (0x47af70) and callers");
        dumpAt(0x0047af70L);
        dumpCallers(0x0047af70L);
        header("TERRAIN -- _G_TileInit (0x447a40)");
        dumpAt(0x00447a40L);
        header("TERRAIN -- do_use_terrain_detail (0x4d2344) and callers");
        dumpAt(0x004d2344L);
        dumpCallers(0x004d2344L);
        header("TERRAIN -- expandTerrain (0x50e145)");
        dumpAt(0x0050e145L);
        header("TERRAIN -- range 0x4D0000-0x4EFFFF");
        dumpRange(0x004d0000L, 0x004effffL);
        header("COLLIDE -- collision symbols");
        dumpSymbolsMatching("_collide", "collide", "_collision", "collision",
                "_impact", "impact", "_hitcheck", "hitcheck", "_checkground",
                "checkground", "_groundcheck", "groundcheck",
                "_intersect", "intersect", "_overlap", "overlap");
        header("COLLIDE -- callers of _DAMAGEDoHit (0x40f970)");
        dumpCallers(0x0040f970L);
        header("COLLIDE -- ?PROJDamageProc (0x4c1870) and callers");
        dumpAt(0x004c1870L);
        dumpCallers(0x004c1870L);
        header("PT -- aircraft performance type field scan 0x00-0xC0");
        findFunctionsReadingOffsets(0x00400000L, 0x00540000L, 0x00, 0xC0);
        header("PT -- PT loader and init");
        dumpSymbolsMatching("_ptload", "ptload", "_loadpt", "loadpt",
                "_ptinit", "ptinit", "_ptread", "ptread", "_ptparse", "ptparse");
        searchStrings(new String[]{".PT", ".pt", "PT\0"});
        closeOutput();
    }
}
