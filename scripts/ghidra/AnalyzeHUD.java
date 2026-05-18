// Consolidates: DumpBit14Targeted, DumpHUDBit14, DumpHUDBit14Search,
//               DumpHUDFunctions, DumpHUDGap, DumpHUDHVel,
//               DumpHUDLoader, DumpHUDWarningBits, DumpHUDWarningBits2

public class AnalyzeHUD extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeHUD");

        // Core draw functions
        header("HUDDrawHeading (0x407b60)");
        dumpAt(0x00407b60L);

        header("HUD draw dispatcher / init (0x406040)");
        dumpAt(0x00406040L);

        header("HUDDrawHVel");
        dumpAt(0x00408060L);  // approximate â€” range scan will catch it

        header("HUDDrawDisrupt (0x40abb0)");
        dumpAt(0x0040abb0L);

        // Full HUD draw range
        header("HUD draw functions 0x407b60-0x40ac00");
        dumpRange(0x00407b60L, 0x0040ac00L);

        // Warning lights + status bitmask (DAT_0050cfef)
        header("FUN_00407930 (warning lights)");
        dumpAt(0x00407930L);

        header("FUN_00407ee0");
        dumpAt(0x00407ee0L);

        header("FUN_00408420");
        dumpAt(0x00408420L);

        header("FUN_00407a00");
        dumpAt(0x00407a00L);

        header("FUN_00416380");
        dumpAt(0x00416380L);

        // All writers to DAT_0050cfef
        header("All writers to DAT_0050cfef (HUD status bitmask)");
        dumpXrefsToData(0x0050cfefL, true);

        // _PLANECheckFuel@0 â€” bit 14/15-18 source
        header("_PLANECheckFuel@0 (0x49fb70)");
        dumpAt(0x0049fb70L);

        // Callers of _HARDPtrs@12 (0x452770) â€” reads hardpoint fuel state
        header("Callers of _HARDPtrs@12 (0x452770)");
        dumpCallers(0x00452770L);

        // Callers of FUN_00452140
        header("Callers of FUN_00452140");
        dumpCallers(0x00452140L);

        // 0x4000 constant scan â€” bit 14 setter
        header("Functions using constant 0x4000 (bit 14) in 0x400000-0x500000");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00500000L, 0x4000L)) dumpAt(va);

        // HUD init gap globals
        header("Xrefs to HUD gap 0x521598-0x5215c3");
        for (long g = 0x00521598L; g <= 0x005215c3L; g += 1) dumpXrefsToData(g, false);

        header("Xrefs to mystery globals 0x5215c5-0x5215cb");
        for (long g = 0x005215c5L; g <= 0x005215cbL; g += 1) dumpXrefsToData(g, false);

        // Gauge globals and DAT_00521541
        header("Writers to DAT_00521541");
        dumpXrefsToData(0x00521541L, true);

        // _PROJProc + PROJMoveProc (sources of HUD missile state)
        header("_PROJProc (0x4c1f50)");
        dumpAt(0x004c1f50L);

        header("PROJMoveProc (0x4c11b0)");
        dumpAt(0x004c11b0L);

        // Bit 14 SP writer — two functions at 0x4bc177 and 0x4bc190 are the unresolved
        // single-player path that writes bit 14 of DAT_0050cfef. These are read during
        // ejection states 0x11 and 0x12. Force-create since they may not be auto-named.
        header("Bit 14 SP writer FUN_004bc177 (ejection state 0x11/0x12)");
        dumpAtForced(0x004bc177L);

        header("Bit 14 SP writer FUN_004bc190");
        dumpAtForced(0x004bc190L);

        header("Callers of FUN_004bc177");
        dumpCallers(0x004bc177L);

        header("Callers of FUN_004bc190");
        dumpCallers(0x004bc190L);

        // Ejection state machine — bits 0x11 and 0x12 select eject phase
        header("Ejection state scan: constant 0x11 in 0x4b8000-0x4c0000");
        for (long va : findFunctionsWithMask(0x004b8000L, 0x004c0000L, 0x11L)) dumpAt(va);

        header("Ejection state scan: constant 0x12 in 0x4b8000-0x4c0000");
        for (long va : findFunctionsWithMask(0x004b8000L, 0x004c0000L, 0x12L)) dumpAt(va);

        // Symbol search
        header("Symbols matching hud/gauge/warning/indicator/display/eject");
        dumpSymbolsMatching("hud", "gauge", "warning", "indicator", "display",
                "cockpit", "eject", "pilot", "escape", "canopy");

        closeOutput();
    }

    private void dumpXrefsToData(long va, boolean writesOnly) throws Exception {
        ghidra.program.model.address.Address addr = toAddr(va);
        for (ghidra.program.model.symbol.Reference ref :
                currentProgram.getReferenceManager().getReferencesTo(addr)) {
            if (writesOnly && !ref.getReferenceType().isWrite()) continue;
            ghidra.program.model.listing.Function fn =
                    currentProgram.getFunctionManager().getFunctionContaining(ref.getFromAddress());
            if (fn != null) dumpAt(fn.getEntryPoint().getOffset());
        }
    }
}
