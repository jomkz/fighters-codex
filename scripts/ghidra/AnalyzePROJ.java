// Consolidates: DumpPROJDispatch, DumpPROJPhysics, DumpPROJPhysics2,
//               DumpPROJPhysics3, DumpPROJPhysicsInit

public class AnalyzePROJ extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzePROJ");

        // Lifecycle
        header("_PROJInit@0 (0x4c06a0)");
        dumpAt(0x004c06a0L);

        header("_PROJAdd@40 (0x4c0a90)");
        dumpAt(0x004c0a90L);

        header("_PROJFire@16 (0x4c2170)");
        dumpAt(0x004c2170L);

        header("_PROJProc (0x4c1f50)");
        dumpAt(0x004c1f50L);

        header("FUN_004c1f10 (contains _PROJProc addr)");
        dumpAt(0x004c1f10L);

        // Movement / physics
        header("?PROJMoveProc (0x4c11b0)");
        dumpAt(0x004c11b0L);

        header("_PROJSpeed@8 (0x4c1120)");
        dumpAt(0x004c1120L);

        header("_PROJEngineState@0 (0x4c1170)");
        dumpAt(0x004c1170L);

        header("_PROJLockUpdate@0 (0x4c0960)");
        dumpAt(0x004c0960L);

        // Hit / damage
        header("_PROJHitChance@28 (0x4c3380)");
        dumpAt(0x004c3380L);

        header("_PROJLock@24 (0x4c2f20)");
        dumpAt(0x004c2f20L);

        header("_DAMAGEDoHit (0x40f970)");
        dumpAt(0x0040f970L);

        // Proximity fuze
        header("Proximity fuze (0x4c3960)");
        dumpAt(0x004c3960L);

        // PROJ_TYPE offset scan (physics gap 0x50-0x7F)
        header("Functions reading PROJ_TYPE offsets 0x50-0x7f in 0x400000-0x500000");
        for (long va : findFunctionsReadingOffsets(0x00400000L, 0x00500000L, 0x50, 0x7F)) dumpAt(va);

        // Entity offset scan 0xF6-0x11E (late entity fields)
        header("Functions reading entity offsets 0xf6-0x11e in 0x400000-0x500000");
        for (long va : findFunctionsReadingOffsets(0x00400000L, 0x00500000L, 0xF6, 0x11E)) dumpAt(va);

        // Warhead flag bits 1-3, 5-6 (missile+0xa6 / PROJ_TYPE+0xa6)
        header("Warhead flag bit tests in 0x4c0000-0x4d0000");
        for (long mask : new long[]{0x2L, 0x4L, 0x8L, 0x20L, 0x40L}) {
            out.println("// -- mask 0x" + Long.toHexString(mask) + " --");
            for (long va : findFunctionsWithMask(0x004c0000L, 0x004d0000L, mask)) dumpAt(va);
        }

        // Full PROJ cluster
        header("All functions 0x4c0500-0x4c2000 (PROJ cluster)");
        dumpRange(0x004c0500L, 0x004c2000L);

        header("All functions 0x4c0000-0x4c3000");
        dumpRange(0x004c0000L, 0x004c3000L);

        closeOutput();
    }
}
