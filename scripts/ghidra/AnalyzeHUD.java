// Consolidates: DumpBit14Targeted, DumpHUDBit14, DumpHUDBit14Search,
//               DumpHUDFunctions, DumpHUDGap, DumpHUDHVel,
//               DumpHUDLoader, DumpHUDWarningBits, DumpHUDWarningBits2

public class AnalyzeHUD extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeHUD");
        header("HUD -- draw dispatcher / init (0x406040)");
        dumpAt(0x00406040L);
        header("HUD -- HUDDrawHeading (0x407b60)");
        dumpAt(0x00407b60L);
        header("HUD -- HUDDrawHVel (0x408060)");
        dumpAt(0x00408060L);
        header("HUD -- HUDDrawDisrupt (0x40abb0)");
        dumpAt(0x0040abb0L);
        header("HUD -- draw functions 0x407b60-0x40ac00");
        dumpRange(0x00407b60L, 0x0040ac00L);
        header("HUD -- FUN_00407930 (warning lights)");
        dumpAt(0x00407930L);
        header("HUD -- FUN_00407ee0");
        dumpAt(0x00407ee0L);
        header("HUD -- FUN_00408420");
        dumpAt(0x00408420L);
        header("HUD -- FUN_00407a00");
        dumpAt(0x00407a00L);
        header("HUD -- FUN_00416380");
        dumpAt(0x00416380L);
        header("HUD -- all writers to DAT_0050cfef (HUD status bitmask)");
        dumpXrefsToData(0x0050cfefL, true);
        header("HUD -- _PLANECheckFuel@0 (0x49fb70)");
        dumpAt(0x0049fb70L);
        header("HUD -- callers of _HARDPtrs@12 (0x452770)");
        dumpCallers(0x00452770L);
        header("HUD -- callers of FUN_00452140");
        dumpCallers(0x00452140L);
        header("HUD -- constant 0x4000 (bit 14) scan 0x400000-0x500000");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00500000L, 0x4000L)) dumpAt(va);
        header("HUD -- xrefs to HUD init gap 0x521598-0x5215c3");
        for (long g = 0x00521598L; g <= 0x005215c3L; g += 1) dumpXrefsToData(g, false);
        header("HUD -- xrefs to mystery globals 0x5215c5-0x5215cb");
        for (long g = 0x005215c5L; g <= 0x005215cbL; g += 1) dumpXrefsToData(g, false);
        header("HUD -- writers to DAT_00521541");
        dumpXrefsToData(0x00521541L, true);
        header("HUD -- _PROJProc (0x4c1f50)");
        dumpAt(0x004c1f50L);
        header("HUD -- PROJMoveProc (0x4c11b0)");
        dumpAt(0x004c11b0L);
        header("HUD -- bit 14 SP writer FUN_004bc177 (ejection state 0x11/0x12)");
        dumpAtForced(0x004bc177L);
        header("HUD -- bit 14 SP writer FUN_004bc190");
        dumpAtForced(0x004bc190L);
        header("HUD -- callers of FUN_004bc177");
        dumpCallers(0x004bc177L);
        header("HUD -- callers of FUN_004bc190");
        dumpCallers(0x004bc190L);
        header("HUD -- ejection state 0x11 scan 0x4b8000-0x4c0000");
        for (long va : findFunctionsWithMask(0x004b8000L, 0x004c0000L, 0x11L)) dumpAt(va);
        header("HUD -- ejection state 0x12 scan 0x4b8000-0x4c0000");
        for (long va : findFunctionsWithMask(0x004b8000L, 0x004c0000L, 0x12L)) dumpAt(va);
        header("HUD -- symbols matching hud/gauge/warning/indicator/display/eject");
        dumpSymbolsMatching("hud", "gauge", "warning", "indicator", "display",
                "cockpit", "eject", "pilot", "escape", "canopy");
        closeOutput();
    }
}
