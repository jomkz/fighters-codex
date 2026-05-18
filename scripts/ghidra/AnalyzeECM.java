// Consolidates: DumpECMEval, DumpECMGeometry, DumpECMPower

public class AnalyzeECM extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeECM");

        // ECM evaluator
        header("FUN_00452770 (ECM evaluator / _HARDPtrs@12)");
        dumpAt(0x00452770L);

        header("Callers of FUN_00452770");
        dumpCallers(0x00452770L);

        // ECM geometry
        header("FUN_004d5e58 (ECM geometry)");
        dumpAt(0x004d5e58L);

        header("Callers of FUN_004d5e58");
        dumpCallers(0x004d5e58L);

        header("FUN_004c39a0");
        dumpAt(0x004c39a0L);

        // Jammer finding
        header("@HARDFindJammer (0x452ea0)");
        dumpAt(0x00452ea0L);

        header("@HARDFindJammer@4");
        dumpSymbolsMatching("hardfindja");

        header("FUN_00452980");
        dumpAt(0x00452980L);

        // ECM power flags â€” bit tests 0x20/0x40/0x80/0xe0
        header("Bit test 0x20 in 0x452000-0x454000");
        for (long va : findFunctionsWithMask(0x00452000L, 0x00454000L, 0x20L)) dumpAt(va);

        header("Bit test 0x40 in 0x452000-0x454000");
        for (long va : findFunctionsWithMask(0x00452000L, 0x00454000L, 0x40L)) dumpAt(va);

        header("Bit test 0x80 in 0x452000-0x454000");
        for (long va : findFunctionsWithMask(0x00452000L, 0x00454000L, 0x80L)) dumpAt(va);

        header("Bit test 0xe0 in 0x452000-0x454000");
        for (long va : findFunctionsWithMask(0x00452000L, 0x00454000L, 0xe0L)) dumpAt(va);

        // Expanded scan in vehicle/GVProc range
        header("ECM bit tests 0x20/0x40/0x80 in 0x4b0000-0x4c0000");
        for (long mask : new long[]{0x20L, 0x40L, 0x80L}) {
            out.println("// mask 0x" + Long.toHexString(mask));
            for (long va : findFunctionsWithMask(0x004b0000L, 0x004c0000L, mask)) dumpAt(va);
        }

        // Damage + lock (ECM cross-reference)
        header("_DAMAGEDoHit (0x40f970)");
        dumpAt(0x0040f970L);

        header("_PROJLockUpdate@0 (0x4c0960)");
        dumpAt(0x004c0960L);

        header("_PROJLock@24 (0x4c2f20)");
        dumpAt(0x004c2f20L);

        header("FUN_004c1870");
        dumpAt(0x004c1870L);

        // Symbol search
        header("Symbols matching ecm/jammer/jam/chaff/flare/counter");
        dumpSymbolsMatching("ecm", "jammer", "jam", "chaff", "flare", "counter", "decoy");

        closeOutput();
    }
}
