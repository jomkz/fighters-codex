// Consolidates: DumpECMEval, DumpECMGeometry, DumpECMPower

public class AnalyzeECM extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeECM");
        header("ECM -- FUN_00452770 (ECM evaluator / _HARDPtrs@12)");
        dumpAt(0x00452770L);
        header("ECM -- callers of FUN_00452770");
        dumpCallers(0x00452770L);
        header("ECM -- FUN_004d5e58 (ECM geometry)");
        dumpAt(0x004d5e58L);
        header("ECM -- callers of FUN_004d5e58");
        dumpCallers(0x004d5e58L);
        header("ECM -- FUN_004c39a0");
        dumpAt(0x004c39a0L);
        header("ECM -- @HARDFindJammer (0x452ea0)");
        dumpAt(0x00452ea0L);
        header("ECM -- @HARDFindJammer@4 (symbol)");
        dumpSymbolsMatching("hardfindja");
        header("ECM -- FUN_00452980");
        dumpAt(0x00452980L);
        header("ECM -- bit 0x20 scan 0x452000-0x454000");
        for (long va : findFunctionsWithMask(0x00452000L, 0x00454000L, 0x20L)) dumpAt(va);
        header("ECM -- bit 0x40 scan");
        for (long va : findFunctionsWithMask(0x00452000L, 0x00454000L, 0x40L)) dumpAt(va);
        header("ECM -- bit 0x80 scan");
        for (long va : findFunctionsWithMask(0x00452000L, 0x00454000L, 0x80L)) dumpAt(va);
        header("ECM -- bit 0xe0 scan");
        for (long va : findFunctionsWithMask(0x00452000L, 0x00454000L, 0xe0L)) dumpAt(va);
        header("ECM -- bits 0x20/0x40/0x80 expanded scan 0x4b0000-0x4c0000");
        for (long mask : new long[]{0x20L, 0x40L, 0x80L}) {
            out.println("// mask 0x" + Long.toHexString(mask));
            for (long va : findFunctionsWithMask(0x004b0000L, 0x004c0000L, mask)) dumpAt(va);
        }
        header("ECM -- _DAMAGEDoHit (0x40f970)");
        dumpAt(0x0040f970L);
        header("ECM -- _PROJLockUpdate@0 (0x4c0960)");
        dumpAt(0x004c0960L);
        header("ECM -- _PROJLock@24 (0x4c2f20)");
        dumpAt(0x004c2f20L);
        header("ECM -- FUN_004c1870");
        dumpAt(0x004c1870L);
        header("ECM -- symbols matching ecm/jammer/jam/chaff/flare/counter");
        dumpSymbolsMatching("ecm", "jammer", "jam", "chaff", "flare", "counter", "decoy");
        closeOutput();
    }
}
