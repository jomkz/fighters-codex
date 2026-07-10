// Consolidates: DumpPROJDispatch, DumpPROJPhysics, DumpPROJPhysics2,
//               DumpPROJPhysics3, DumpPROJPhysicsInit

public class AnalyzePROJ extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzePROJ");
        header("PROJ -- _PROJInit@0 (0x4c06a0)");
        dumpAt(0x004c06a0L);
        header("PROJ -- _PROJAdd@40 (0x4c0a90)");
        dumpAt(0x004c0a90L);
        header("PROJ -- _PROJFire@16 (0x4c2170)");
        dumpAt(0x004c2170L);
        header("PROJ -- _PROJProc (0x4c1f50)");
        dumpAtForced(0x004c1f50L);
        header("PROJ -- FUN_004c1f10 (contains _PROJProc addr)");
        dumpAt(0x004c1f10L);
        header("PROJ -- ?PROJMoveProc (0x4c11b0)");
        dumpAtForced(0x004c11b0L);
        header("PROJ -- _PROJSpeed@8 (0x4c1120)");
        dumpAt(0x004c1120L);
        header("PROJ -- _PROJEngineState@0 (0x4c1170)");
        dumpAt(0x004c1170L);
        header("PROJ -- _PROJLockUpdate@0 (0x4c0960)");
        dumpAt(0x004c0960L);
        header("PROJ -- _PROJHitChance@28 (0x4c3380)");
        dumpAt(0x004c3380L);
        header("PROJ -- _PROJLock@24 (0x4c2f20)");
        dumpAt(0x004c2f20L);
        header("PROJ -- _DAMAGEDoHit (0x40f970)");
        dumpAt(0x0040f970L);
        header("PROJ -- proximity fuze (0x4c3960)");
        dumpAt(0x004c3960L);
        header("PROJ -- PROJ_TYPE offset scan 0x50-0x7f");
        for (long va : findFunctionsReadingOffsets(0x00400000L, 0x00500000L, 0x50, 0x7F)) dumpAt(va);
        header("PROJ -- entity offset scan 0xf6-0x11e");
        for (long va : findFunctionsReadingOffsets(0x00400000L, 0x00500000L, 0xF6, 0x11E)) dumpAt(va);
        header("PROJ -- warhead flag bit tests in 0x4c0000-0x4d0000");
        for (long mask : new long[]{0x2L, 0x4L, 0x8L, 0x20L, 0x40L}) {
            out.println("// -- mask 0x" + Long.toHexString(mask) + " --");
            for (long va : findFunctionsWithMask(0x004c0000L, 0x004d0000L, mask)) dumpAt(va);
        }
        header("PROJ -- all functions 0x4c0000-0x4c3000");
        dumpRange(0x004c0000L, 0x004c3000L);
        closeOutput();
    }
}
