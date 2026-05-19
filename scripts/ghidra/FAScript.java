import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.*;
import ghidra.program.model.mem.*;
import java.io.*;
import java.util.*;

public abstract class FAScript extends GhidraScript {

    protected DecompInterface decompiler;
    protected PrintWriter out;
    protected Set<Long> dumped;
    protected String outputPath;

    @Override
    public void run() throws Exception {}

    // -----------------------------------------------------------------------
    // Output lifecycle
    // -----------------------------------------------------------------------

    protected void openOutput(String name) throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        dumped = new LinkedHashSet<>();
        String projectDir = System.getenv("FA_PROJECT");
        if (projectDir == null || projectDir.isEmpty())
            projectDir = System.getProperty("java.io.tmpdir");
        File outDir = new File(projectDir, "output");
        outDir.mkdirs();
        File outFile = new File(outDir, name + ".txt");
        outputPath = outFile.getAbsolutePath();
        out = new PrintWriter(new FileWriter(outFile));
    }

    /** Append mode: used by overlay-DLL scripts that run once per DLL in a batch. */
    protected void openOutputAppend(String name) throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        dumped = new LinkedHashSet<>();
        String projectDir = System.getenv("FA_PROJECT");
        if (projectDir == null || projectDir.isEmpty())
            projectDir = System.getProperty("java.io.tmpdir");
        File outDir = new File(projectDir, "output");
        outDir.mkdirs();
        File outFile = new File(outDir, name + ".txt");
        outputPath = outFile.getAbsolutePath();
        out = new PrintWriter(new FileWriter(outFile, true));   // append
        out.println("// ============================================================");
        out.println("// " + name + " -- " + currentProgram.getName());
        out.println("// Image base: 0x"
                + Long.toHexString(currentProgram.getImageBase().getOffset()));
        out.println("// ============================================================");
        out.println();
    }

    protected void closeOutput() {
        if (out != null) {
            out.close(); out = null;
            println("Output: " + outputPath);
        }
        if (decompiler != null) { decompiler.dispose(); decompiler = null; }
    }

    protected void header(String title) {
        out.println("\n// === " + title + " ===");
    }

    // -----------------------------------------------------------------------
    // Function dump helpers
    // -----------------------------------------------------------------------

    protected void dumpAt(long va) throws Exception {
        if (dumped.contains(va)) return;
        dumped.add(va);
        Address addr = toAddr(va);
        FunctionManager fm = currentProgram.getFunctionManager();
        Function fn = fm.getFunctionAt(addr);
        if (fn == null) fn = fm.getFunctionContaining(addr);
        if (fn == null) {
            out.println("// NOT FOUND at 0x" + Long.toHexString(va));
            return;
        }
        dumpFunction(fn);
    }

    protected void dumpAtForced(long va) throws Exception {
        if (dumped.contains(va)) return;
        dumped.add(va);
        Address addr = toAddr(va);
        FunctionManager fm = currentProgram.getFunctionManager();
        Function fn = fm.getFunctionAt(addr);
        if (fn == null) {
            DisassembleCommand disCmd = new DisassembleCommand(addr, null, true);
            disCmd.applyTo(currentProgram, monitor);
            CreateFunctionCmd createCmd = new CreateFunctionCmd(addr);
            createCmd.applyTo(currentProgram, monitor);
            fn = fm.getFunctionAt(addr);
        }
        if (fn == null) {
            out.println("// STILL NOT FOUND at 0x" + Long.toHexString(va));
            return;
        }
        dumpFunction(fn);
    }

    protected void dumpFunction(Function fn) throws Exception {
        out.println("// --- " + fn.getName() + " @ " + fn.getEntryPoint() + " ---");
        DecompileResults res = decompiler.decompileFunction(fn, 120, monitor);
        if (res != null && res.getDecompiledFunction() != null) {
            out.println(res.getDecompiledFunction().getC());
        } else {
            out.println("// decompile failed: " + fn.getName());
        }
        out.println();
    }

    protected void dumpCallers(long va) throws Exception {
        Address addr = toAddr(va);
        FunctionManager fm = currentProgram.getFunctionManager();
        Function target = fm.getFunctionAt(addr);
        if (target == null) target = fm.getFunctionContaining(addr);
        String name = target != null ? target.getName() : "0x" + Long.toHexString(va);
        out.println("// Callers of " + name + " (0x" + Long.toHexString(va) + "):");
        for (long callerVa : findCallers(va)) {
            Function fn = fm.getFunctionAt(toAddr(callerVa));
            if (fn != null) out.println("//   " + fn.getName() + " @ 0x" + Long.toHexString(callerVa));
        }
        for (long callerVa : findCallers(va)) dumpAt(callerVa);
    }

    /** Dump all functions that hold data references to the given address. */
    protected void dumpXrefsToData(long va) throws Exception {
        dumpXrefsToData(va, false);
    }

    /** Dump functions that hold data references to the given address.
     *  @param writesOnly  when true only WRITE references are followed */
    protected void dumpXrefsToData(long va, boolean writesOnly) throws Exception {
        Address addr = toAddr(va);
        for (Reference ref : currentProgram.getReferenceManager().getReferencesTo(addr)) {
            if (writesOnly && !ref.getReferenceType().isWrite()) continue;
            Function fn = currentProgram.getFunctionManager()
                    .getFunctionContaining(ref.getFromAddress());
            if (fn != null) dumpAt(fn.getEntryPoint().getOffset());
        }
    }

    protected Set<Long> findCallers(long va) throws Exception {
        Address addr = toAddr(va);
        Set<Long> callers = new LinkedHashSet<>();
        for (Reference ref : currentProgram.getReferenceManager().getReferencesTo(addr)) {
            if (ref.getReferenceType().isCall()) {
                Function fn = currentProgram.getFunctionManager()
                        .getFunctionContaining(ref.getFromAddress());
                if (fn != null) callers.add(fn.getEntryPoint().getOffset());
            }
        }
        return callers;
    }

    protected Set<Long> findFunctionsWithMask(long start, long end, long mask) throws Exception {
        AddressSpace space = currentProgram.getAddressFactory().getDefaultAddressSpace();
        Listing listing = currentProgram.getListing();
        Address endAddr = space.getAddress(end);
        Set<Long> seen = new LinkedHashSet<>();
        InstructionIterator instrs = listing.getInstructions(space.getAddress(start), true);
        while (instrs.hasNext()) {
            Instruction instr = instrs.next();
            if (instr.getAddress().compareTo(endAddr) > 0) break;
            String mnem = instr.getMnemonicString().toLowerCase();
            if (!mnem.equals("and") && !mnem.equals("test") && !mnem.equals("or")) continue;
            for (int i = 0; i < instr.getNumOperands(); i++) {
                for (Object obj : instr.getOpObjects(i)) {
                    if (obj instanceof Scalar) {
                        long val = ((Scalar) obj).getUnsignedValue();
                        if (val == mask) {
                            Function fn = currentProgram.getFunctionManager()
                                    .getFunctionContaining(instr.getAddress());
                            if (fn != null) seen.add(fn.getEntryPoint().getOffset());
                        }
                    }
                }
            }
        }
        return seen;
    }

    protected Set<Long> findFunctionsReadingOffsets(long start, long end, int minOff, int maxOff) throws Exception {
        AddressSpace space = currentProgram.getAddressFactory().getDefaultAddressSpace();
        Listing listing = currentProgram.getListing();
        Address endAddr = space.getAddress(end);
        Set<Long> seen = new LinkedHashSet<>();
        InstructionIterator instrs = listing.getInstructions(space.getAddress(start), true);
        while (instrs.hasNext()) {
            Instruction instr = instrs.next();
            if (instr.getAddress().compareTo(endAddr) > 0) break;
            for (int i = 0; i < instr.getNumOperands(); i++) {
                for (Object obj : instr.getOpObjects(i)) {
                    if (obj instanceof Scalar) {
                        long val = ((Scalar) obj).getUnsignedValue();
                        if (val >= minOff && val <= maxOff) {
                            Function fn = currentProgram.getFunctionManager()
                                    .getFunctionContaining(instr.getAddress());
                            if (fn != null && seen.add(fn.getEntryPoint().getOffset())) {
                                out.println("// offset 0x" + Long.toHexString(val)
                                        + " in " + fn.getName() + " @ " + fn.getEntryPoint());
                            }
                        }
                    }
                }
            }
        }
        return seen;
    }

    protected void searchBitTestsInRange(long start, long end, long mask) throws Exception {
        for (long va : findFunctionsWithMask(start, end, mask)) {
            Function fn = currentProgram.getFunctionManager().getFunctionAt(toAddr(va));
            if (fn == null) fn = currentProgram.getFunctionManager().getFunctionContaining(toAddr(va));
            String name = fn != null ? fn.getName() : "?";
            out.println("// mask 0x" + Long.toHexString(mask) + " in " + name + " @ 0x" + Long.toHexString(va));
            dumpAt(va);
        }
    }

    /** Search for ASCII strings anywhere in the program's address space. */
    protected void searchStrings(String[] patterns) throws Exception {
        Address searchStart = currentProgram.getMinAddress();
        Address searchEnd   = currentProgram.getMaxAddress();
        ReferenceManager rm = currentProgram.getReferenceManager();
        FunctionManager fm  = currentProgram.getFunctionManager();
        for (String kw : patterns) {
            byte[] kwBytes = kw.getBytes("US-ASCII");
            Address found = currentProgram.getMemory().findBytes(
                    searchStart, searchEnd, kwBytes, null, true, monitor);
            while (found != null) {
                out.println("// string '" + kw + "' at " + found);
                for (Reference ref : rm.getReferencesTo(found)) {
                    Function f = fm.getFunctionContaining(ref.getFromAddress());
                    if (f != null && dumped.add(f.getEntryPoint().getOffset())) {
                        out.println("//   ref from " + ref.getFromAddress()
                                + " in " + f.getName() + " @ " + f.getEntryPoint());
                        dumpAt(f.getEntryPoint().getOffset());
                    }
                }
                found = currentProgram.getMemory().findBytes(
                        found.add(1), searchEnd, kwBytes, null, true, monitor);
            }
        }
    }

    protected void dumpRange(long start, long end) throws Exception {
        for (Function fn : currentProgram.getFunctionManager().getFunctions(true)) {
            long ep = fn.getEntryPoint().getOffset();
            if (ep >= start && ep <= end && !dumped.contains(ep)) dumpAt(ep);
        }
    }

    protected void dumpSymbolsMatching(String... keywords) throws Exception {
        for (Symbol sym : currentProgram.getSymbolTable().getAllSymbols(false)) {
            String name = sym.getName().toLowerCase();
            for (String kw : keywords) {
                if (name.contains(kw)) {
                    out.println("// SYM: " + sym.getName() + " @ " + sym.getAddress());
                    Function fn = currentProgram.getFunctionManager().getFunctionAt(sym.getAddress());
                    if (fn == null) fn = currentProgram.getFunctionManager().getFunctionContaining(sym.getAddress());
                    if (fn != null) dumpAt(fn.getEntryPoint().getOffset());
                    break;
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // LAY -- sky / atmosphere layer system
    // Consolidates: DumpLAYFunctions, DumpLAYFunctions2-4, DumpLAYGaps,
    //               DumpLAYGradient, DumpLAYRemaining, DumpLAYStructure
    // -----------------------------------------------------------------------
    protected void analyzeLAY() throws Exception {
        header("LAY -- ParseLayerFile (0x4b4370)");
        dumpAt(0x004b4370L);
        header("LAY -- CopyLayersToRuntime (0x4b3750)");
        dumpAt(0x004b3750L);
        header("LAY -- InterpolateLayers (0x4b3820)");
        dumpAt(0x004b3820L);
        header("LAY -- GetLayerAtAltitude (0x4b3be0)");
        dumpAt(0x004b3be0L);
        header("LAY -- T_DefaultHorizon (0x4aacf0)");
        dumpAt(0x004aacf0L);
        header("LAY -- ApplyBrightnessGradient (0x4b3cb0)");
        dumpAt(0x004b3cb0L);
        header("LAY -- UpdateSkyState (0x4b3d90)");
        dumpAt(0x004b3d90L);
        header("LAY -- UpdateAuroraClouds (0x4b4170)");
        dumpAt(0x004b4170L);
        header("LAY -- FindNearestColorEntry (0x4b3ad0)");
        dumpAt(0x004b3ad0L);
        header("LAY -- LoadPICByWildcard (0x4b4680)");
        dumpAt(0x004b4680L);
        header("LAY -- GetLayerBoundary (0x4b3190)");
        dumpAt(0x004b3190L);
        header("LAY -- GetLayerByIndex (0x4b3170)");
        dumpAt(0x004b3170L);
        header("LAY -- WRFogLayerUpdate (0x4b4320)");
        dumpAt(0x004b4320L);
        header("LAY -- FUN_004b4790");
        dumpAt(0x004b4790L);
        header("LAY -- hdrPtr writer 1 (0x4b46d0)");
        dumpAt(0x004b46d0L);
        header("LAY -- interpolation cluster 0x4b3b60-0x4b3d90");
        dumpAt(0x004b3b60L); dumpAt(0x004b3b80L); dumpAt(0x004b4680L);
        dumpAt(0x004b46f0L); dumpAt(0x004b4700L); dumpAt(0x004b4720L);
        header("LAY -- callers of ParseLayerFile");
        dumpCallers(0x004b4370L);
        header("LAY -- callers of FindNearestColorEntry");
        dumpCallers(0x004b3ad0L);
        header("LAY -- callers of WRFogLayerUpdate");
        dumpCallers(0x004b4320L);
        header("LAY -- callers of FUN_004b3410");
        dumpCallers(0x004b3410L);
        header("LAY -- xrefs to hdrPtr (DAT_00580d94)");
        dumpXrefsToData(0x00580d94L);
        header("LAY -- xrefs to curLayers (DAT_00583250)");
        dumpXrefsToData(0x00583250L);
        header("LAY -- xrefs to DAT_00583da8");
        dumpXrefsToData(0x00583da8L);
        header("LAY -- xrefs to gap globals 0x580dc4-0x580e18");
        for (long g = 0x00580dc4L; g <= 0x00580e18L; g += 4) dumpXrefsToData(g);
        header("LAY -- all functions 0x4b2ea0-0x4b4200");
        dumpRange(0x004b2ea0L, 0x004b4200L);
        header("LAY -- symbols matching cloud/layer/horizon/fog/weather/gradient");
        dumpSymbolsMatching("cloud", "layer", "horizon", "fog", "weather", "gradient",
                "lay", "aurora", "sky", "haze");
    }

    // -----------------------------------------------------------------------
    // HUD -- heads-up display
    // Consolidates: DumpBit14Targeted, DumpHUDBit14, DumpHUDBit14Search,
    //               DumpHUDFunctions, DumpHUDGap, DumpHUDHVel,
    //               DumpHUDLoader, DumpHUDWarningBits, DumpHUDWarningBits2
    // -----------------------------------------------------------------------
    protected void analyzeHUD() throws Exception {
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
    }

    // -----------------------------------------------------------------------
    // DLG -- dialog / UI system
    // Consolidates: DumpDLGChoosePreload, DumpDLGDispatch, DumpDLGDispatch2,
    //               DumpDLGDispatch3, DumpDLGDispatcher, DumpDLGFunctions,
    //               DumpDLGHelpers
    // -----------------------------------------------------------------------
    protected void analyzeDLG() throws Exception {
        header("DLG -- _DialogWhatItem (0x488fc0)");
        dumpAtForced(0x00488fc0L);
        header("DLG -- _DialogSetup (0x487a63)");
        dumpAt(0x00487a63L);
        header("DLG -- _DialogShow (0x4880d0)");
        dumpAt(0x004880d0L);
        header("DLG -- _DialogShutDown (0x488190)");
        dumpAt(0x00488190L);
        header("DLG -- _DialogDone (0x488300)");
        dumpAt(0x00488300L);
        header("DLG -- @DialogGetPtr (0x4892e0)");
        dumpAt(0x004892e0L);
        header("DLG -- @DialogGetValue (0x489300)");
        dumpAt(0x00489300L);
        header("DLG -- @DialogSelectItem (0x4894f0)");
        dumpAt(0x004894f0L);
        header("DLG -- @DialogDeselectItem (0x489580)");
        dumpAtForced(0x00489580L);
        header("DLG -- _TopCenterDialog (0x489710)");
        dumpAtForced(0x00489710L);
        header("DLG -- _ChoosePreload (0x4897f0)");
        dumpAt(0x004897f0L);
        header("DLG -- _ChoosePreload helpers");
        dumpAt(0x00489830L); dumpAt(0x00489870L);
        dumpAt(0x004897c0L); dumpAt(0x00489840L);
        header("DLG -- callers of func_0x00489840");
        dumpCallers(0x00489840L);
        header("DLG -- _ChooseActivity (0x4a08a0)");
        dumpAt(0x004a08a0L);
        header("DLG -- _DrawText (0x489ac0)");
        dumpAt(0x00489ac0L);
        header("DLG -- _DrawAction (0x489b90)");
        dumpAt(0x00489b90L);
        header("DLG -- _DrawFormattedText (0x48a910)");
        dumpAt(0x0048a910L);
        header("DLG -- _DrawCampaignList (0x48abf0)");
        dumpAt(0x0048abf0L);
        header("DLG -- _DrawRocker (0x48b4e0)");
        dumpAt(0x0048b4e0L);
        header("DLG -- _DrawEditBox (0x48c710)");
        dumpAt(0x0048c710L);
        header("DLG -- FUN_004a6e20 (dispatcher)");
        dumpAt(0x004a6e20L);
        header("DLG -- callers of dispatcher");
        dumpCallers(0x004a6e20L);
        header("DLG -- forced cluster 0x488470-0x4a0810");
        dumpAtForced(0x00488470L); dumpAtForced(0x00488490L); dumpAtForced(0x004a0810L);
        dumpAt(0x0040bd30L); dumpAt(0x00436280L); dumpAt(0x004891a0L);
        dumpAt(0x00488f50L); dumpAt(0x0048d0d0L); dumpAt(0x0048d140L);
        header("DLG -- FUN_0040d5f0");
        dumpAt(0x0040d5f0L);
        header("DLG -- FUN_0048b2e0");
        dumpAt(0x0048b2e0L);
        header("DLG -- callers of _ChoosePreload");
        dumpCallers(0x004897f0L);
        header("DLG -- symbols matching dialog/dlg/choose/screen/panel/button/item");
        dumpSymbolsMatching("dialog", "dlg", "choose", "screen", "panel", "button", "item",
                "preload", "activity", "rocker", "editbox");
    }

    // -----------------------------------------------------------------------
    // PROJ -- projectile / missile system
    // Consolidates: DumpPROJDispatch, DumpPROJPhysics, DumpPROJPhysics2,
    //               DumpPROJPhysics3, DumpPROJPhysicsInit
    // -----------------------------------------------------------------------
    protected void analyzePROJ() throws Exception {
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
    }

    // -----------------------------------------------------------------------
    // SEE -- seeker / missile guidance
    // Consolidates: DumpSEEAndJT, DumpSEETransition, DumpSEETransition2,
    //               DumpSEETransition3
    // -----------------------------------------------------------------------
    protected void analyzeSEE() throws Exception {
        header("SEE -- _PROJInFOV / seeker cone (0x4c2860)");
        dumpAt(0x004c2860L);
        header("SEE -- callers of seeker cone");
        dumpCallers(0x004c2860L);
        header("SEE -- search lobe (0x4c2eb0)");
        dumpAt(0x004c2eb0L);
        header("SEE -- track lobe (0x4c31f0)");
        dumpAt(0x004c31f0L);
        header("SEE -- callers of search lobe");
        dumpCallers(0x004c2eb0L);
        header("SEE -- callers of track lobe");
        dumpCallers(0x004c31f0L);
        header("SEE -- pre-lobe (0x4c24b0)");
        dumpAt(0x004c24b0L);
        header("SEE -- guidance final (0x4c26f0)");
        dumpAt(0x004c26f0L);
        header("SEE -- lock-acquire 1 (0x4c5000)");
        dumpAt(0x004c5000L);
        header("SEE -- lock-acquire 2 (0x4c5050)");
        dumpAt(0x004c5050L);
        header("SEE -- target selection (0x4c52d0)");
        dumpAt(0x004c52d0L);
        header("SEE -- _PROJAdd (0x4c0a90)");
        dumpAt(0x004c0a90L);
        header("SEE -- missile flag handler (0x4c3eb0)");
        dumpAt(0x004c3eb0L);
        header("SEE -- re-eval (0x4c4100)");
        dumpAt(0x004c4100L);
        header("SEE -- wrapper (0x4c58a0)");
        dumpAt(0x004c58a0L);
        header("SEE -- proximity fuze (0x4c3960)");
        dumpAt(0x004c3960L);
        header("SEE -- _PROJHitChance@28 (0x4c3380)");
        dumpAt(0x004c3380L);
        header("SEE -- ECM+warhead cluster");
        dumpAt(0x004c2b50L); dumpAt(0x004c3360L); dumpAt(0x004c20c0L); dumpAt(0x004c5670L);
        header("SEE -- 0x20000 flag scan 0x4c0000-0x4c7000");
        for (long va : findFunctionsWithMask(0x004c0000L, 0x004c7000L, 0x20000L)) dumpAt(va);
        header("SEE -- 0x100000 flag scan");
        for (long va : findFunctionsWithMask(0x004c0000L, 0x004c7000L, 0x100000L)) dumpAt(va);
        header("SEE -- 0x10000 flag scan");
        for (long va : findFunctionsWithMask(0x004c0000L, 0x004c7000L, 0x10000L)) dumpAt(va);
        header("SEE -- symbols matching see/seeker/missile/lock/ir/radar/fov/lobe");
        dumpSymbolsMatching("see", "seeker", "missile", "lock", "ir", "radar", "fov", "lobe",
                "prox", "fuze", "warhead", "guidance");
    }

    // -----------------------------------------------------------------------
    // MM -- mission map / campaign
    // Consolidates: DumpMCCAMLoader, DumpMMCAMMission, DumpMMLayerSlot,
    //               DumpMMLayerSlot2, DumpMMLayerSlot3, DumpMMLayerSlot4
    // -----------------------------------------------------------------------
    protected void analyzeMM() throws Exception {
        header("MM -- keyword handler FUN_0047a510");
        dumpAt(0x0047a510L);
        header("MM -- callers of FUN_0047a510");
        dumpCallers(0x0047a510L);
        header("MM -- FUN_0047a130 (MM line parser)");
        dumpAt(0x0047a130L);
        header("MM -- callers of FUN_0047a130");
        dumpCallers(0x0047a130L);
        header("MM -- _MISSIONInit2 variant 1 (0x480b50)");
        dumpAt(0x00480b50L);
        header("MM -- _MISSIONInit2 variant 2 (0x480a30)");
        dumpAt(0x00480a30L);
        header("MM -- callers of _MISSIONInit2");
        dumpCallers(0x00480b50L);
        header("MM -- MC loader (0x481940)");
        dumpAt(0x00481940L);
        header("MM -- callers of MC loader");
        dumpCallers(0x00481940L);
        header("MM -- Pre-MC (0x480750)");
        dumpAt(0x00480750L);
        header("MM -- campaign launcher (0x428412)");
        dumpAt(0x00428412L);
        header("MM -- FUN_00481c10");
        dumpAt(0x00481c10L);
        header("MM -- ParseLayerFile (0x4b4370)");
        dumpAt(0x004b4370L);
        header("MM -- FUN_004b3480");
        dumpAt(0x004b3480L);
        header("MM -- GetLayerByIndex (0x4b3170)");
        dumpAt(0x004b3170L);
        header("MM -- callers of GetLayerByIndex");
        dumpCallers(0x004b3170L);
        header("MM -- FUN_0044f180 / FUN_00430a90 / FUN_0043a5c0");
        dumpAt(0x0044f180L); dumpAt(0x00430a90L); dumpAt(0x0043a5c0L);
        header("MM -- keyword helpers 0x4ace50 / 0x4acfa0");
        dumpAt(0x004ace50L); dumpAt(0x004acfa0L);
        header("MM -- all functions 0x481800-0x482200");
        dumpRange(0x00481800L, 0x00482200L);
        header("MM -- strings: MM keywords and file extensions");
        searchStrings(new String[]{"textFormat", "map", ".T2", ".LAY", "LAY ", "tmap", "tdic",
                "mission", ".MM", "theater"});
        header("MM -- symbols matching mm/map/mission/campaign/cam/mc/theater");
        dumpSymbolsMatching("mm", "map", "mission", "campaign", "cam", "theater",
                "mcload", "mapload", "mapinit");
    }

    // -----------------------------------------------------------------------
    // BI -- bytecode interpreter / AI scripts
    // Consolidates: DumpAIScripts, DumpCTOpcodeArgs, DumpPlaneCheckFuelCT
    // -----------------------------------------------------------------------
    protected void analyzeBI() throws Exception {
        header("BI -- _CTExecProgram@4 (0x466970)");
        dumpAt(0x00466970L);
        header("BI -- callers of _CTExecProgram@4");
        dumpCallers(0x00466970L);
        header("BI -- range near interpreter 0x466800-0x466a00");
        dumpRange(0x00466800L, 0x00466a00L);
        header("BI -- opcode dispatcher FUN_00466a80");
        dumpAtForced(0x00466a80L);
        header("BI -- _CTDo_move (0x465cc0)");
        dumpAtForced(0x00465cc0L);
        header("BI -- _CTDo_movetoalt (0x465e20)");
        dumpAtForced(0x00465e20L);
        header("BI -- _CTDo_jink (0x4663f0)");
        dumpAtForced(0x004663f0L);
        header("BI -- _CTDo_maneuver (0x465a70)");
        dumpAtForced(0x00465a70L);
        header("BI -- _CTDo_turn (0x465ea0)");
        dumpAtForced(0x00465ea0L);
        header("BI -- arg reader range 0x465c00-0x465f00");
        dumpRange(0x00465c00L, 0x00465f00L);
        dumpAtForced(0x00465c90L); dumpAtForced(0x00465d40L); dumpAtForced(0x00465da0L);
        dumpAtForced(0x00465de0L); dumpAtForced(0x00465e00L);
        header("BI -- _CTEval_ir (0x4650e0)");
        dumpAt(0x004650e0L);
        header("BI -- _CTEval_radar (0x4650a0)");
        dumpAt(0x004650a0L);
        header("BI -- _CTEval_do_ir_launch (0x464e70)");
        dumpAt(0x00464e70L);
        header("BI -- _CTEval_do_radar_launch (0x464e60)");
        dumpAt(0x00464e60L);
        header("BI -- _MVRJink@40 (0x4ac9e0)");
        dumpAt(0x004ac9e0L);
        header("BI -- _MVRMove (0x4ac510)");
        dumpAt(0x004ac510L);
        header("BI -- _CreateMove (0x463a20)");
        dumpAt(0x00463a20L);
        header("BI -- _CreateMoveGoal (0x463af0)");
        dumpAt(0x00463af0L);
        header("BI -- @WriteCmdBufMove (0x463cc0)");
        dumpAt(0x00463cc0L);
        header("BI -- _MISSIONInit2 (0x480b50)");
        dumpAt(0x00480b50L);
        header("BI -- callers of _MISSIONInit2");
        dumpCallers(0x00480b50L);
        header("BI -- all _CTDo_* / _CTEval_* symbols");
        dumpSymbolsMatching("ctdo_", "cteval_", "ctexec", "cttry", "ctplan");
        // FRAME opcode 0x28 reader search
        header("BI -- FRAME reader: xrefs to DAT_00546c44");
        dumpXrefsToData(0x00546c44L);
        header("BI -- FRAME reader: xrefs to DAT_00546c46");
        dumpXrefsToData(0x00546c46L);
        header("BI -- wide scan for reads of +0x7c/0x7e (CT state FRAME fields)");
        for (long va : findFunctionsReadingOffsets(0x00460000L, 0x00490000L,
                0x546c44 & 0xFF, 0x546c46 & 0xFF)) dumpAt(va);
        // BI runtime state globals
        header("BI -- xrefs to IP (DAT_00546bea)");
        dumpXrefsToData(0x00546beaL);
        header("BI -- xrefs to priority (DAT_00546bf0)");
        dumpXrefsToData(0x00546bf0L);
        header("BI -- xrefs to actor (DAT_00546c94)");
        dumpXrefsToData(0x00546c94L);
        header("BI -- xrefs to halt flag (DAT_00546c98)");
        dumpXrefsToData(0x00546c98L);
    }

    // -----------------------------------------------------------------------
    // ECM -- electronic countermeasures
    // Consolidates: DumpECMEval, DumpECMGeometry, DumpECMPower
    // -----------------------------------------------------------------------
    protected void analyzeECM() throws Exception {
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
    }

    // -----------------------------------------------------------------------
    // HGR -- hangar / airbase rendering
    // Consolidates: DumpHGRFormat, DumpHGRLoader, DumpHGRT2Bit14
    // -----------------------------------------------------------------------
    protected void analyzeHGR() throws Exception {
        header("HGR -- FUN_004543c0 (loader)");
        dumpAt(0x004543c0L);
        header("HGR -- callers of loader");
        dumpCallers(0x004543c0L);
        header("HGR -- FUN_004809d0");
        dumpAt(0x004809d0L);
        header("HGR -- FUN_00480150");
        dumpAt(0x00480150L);
        header("HGR -- FUN_004801a0");
        dumpAt(0x004801a0L);
        header("HGR -- range 0x480000-0x480600");
        dumpRange(0x00480000L, 0x00480600L);
        header("HGR -- HUD/hangar draw range 0x406000-0x406200");
        dumpRange(0x00406000L, 0x00406200L);
        header("HGR -- xrefs to DAT_004fbbf0");
        dumpXrefsToData(0x004fbbf0L);
        header("HGR -- xrefs to hangarName (0x4fb1e8)");
        dumpXrefsToData(0x004fb1e8L);
        header("HGR -- _G_TileInit (0x447a40)");
        dumpAt(0x00447a40L);
        header("HGR -- @G_Tile@32 (0x447aa5)");
        dumpAt(0x00447aa5L);
        header("HGR -- _T_Init2 (0x4c5f60)");
        dumpAt(0x004c5f60L);
        header("HGR -- tileExpand (0x4f4f78)");
        dumpAt(0x004f4f78L);
        header("HGR -- _expandTerrain (0x50e145)");
        dumpAt(0x0050e145L);
        header("HGR -- bit 14 (0x4000) scan 0x400000-0x550000");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x4000L)) dumpAt(va);
        header("HGR -- symbols matching hgr/hangar/airbase/h_airb/t2/terrain/tile/tileexpand");
        dumpSymbolsMatching("hgr", "hangar", "airbase", "h_airb", "t2", "terrain", "tile",
                "tileexpand", "expandterrain");
    }

    // -----------------------------------------------------------------------
    // MUS -- music / sound sequencer
    // Consolidates: DumpMUSCallbacks, DumpMUSInterpreter, DumpSEQFunctions
    // -----------------------------------------------------------------------
    protected void analyzeMUS() throws Exception {
        header("MUS -- _SEQmusic (0x446b70)");
        dumpAtForced(0x00446b70L);
        header("MUS -- _SEQfadein (0x446890)");
        dumpAtForced(0x00446890L);
        header("MUS -- _SEQfadeout (0x446910)");
        dumpAtForced(0x00446910L);
        header("MUS -- ?SeqFadeOut (0x446990)");
        dumpAtForced(0x00446990L);
        header("MUS -- ?SeqFadeIn (0x4469f0)");
        dumpAtForced(0x004469f0L);
        header("MUS -- ?MusicOn (0x4329e0)");
        dumpAt(0x004329e0L);
        header("MUS -- ?MusicOff (0x432c00)");
        dumpAt(0x00432c00L);
        header("MUS -- ?DMusicOff (0x432bd0)");
        dumpAt(0x00432bd0L);
        header("MUS -- ?DMusicVolume (0x432a80)");
        dumpAt(0x00432a80L);
        header("MUS -- ?MusicVolume (0x432b40)");
        dumpAt(0x00432b40L);
        header("MUS -- ?ShellMusicUpdate (0x432f80)");
        dumpAt(0x00432f80L);
        header("MUS -- ?ShellMusic (0x433170)");
        dumpAt(0x00433170L);
        header("MUS -- ?InitMusic (0x4328b0)");
        dumpAtForced(0x004328b0L);
        header("MUS -- ?ShutDownMidi (0x432920)");
        dumpAtForced(0x00432920L);
        header("MUS -- loader 1 (0x4a6ae0)");
        dumpAt(0x004a6ae0L);
        header("MUS -- loader 2 (0x4a6b50)");
        dumpAt(0x004a6b50L);
        header("MUS -- loader 3 (0x4a7180)");
        dumpAt(0x004a7180L);
        header("MUS -- AIL callback registration");
        dumpSymbolsMatching("ail_init", "ail_register", "ailcallback", "ail_sequence");
        searchStrings(new String[]{"AIL_init_sequence", "AIL_register"});
        header("MUS -- symbols matching mus/music/seq/midi/sound/audio/ail");
        dumpSymbolsMatching("mus", "music", "seq", "midi", "sound", "audio", "ail", "fade");
    }

    // -----------------------------------------------------------------------
    // OTNT -- OT/NT vehicle classification and AI
    // Consolidates: DumpOTNTFlags, DumpOTNTFlags2
    // -----------------------------------------------------------------------
    protected void analyzeOTNT() throws Exception {
        header("OTNT -- _GVProc (0x473db0)");
        dumpAt(0x00473db0L);
        header("OTNT -- FUN_004bed70");
        dumpAt(0x004bed70L);
        header("OTNT -- FUN_004747c0");
        dumpAt(0x004747c0L);
        header("OTNT -- priority mask 0x8000");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x8000L)) dumpAt(va);
        header("OTNT -- priority mask 0x40000");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x40000L)) dumpAt(va);
        header("OTNT -- priority mask 0x80000");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x80000L)) dumpAt(va);
        header("OTNT -- priority mask 0x100000");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x100000L)) dumpAt(va);
        header("OTNT -- priority mask 0x400000");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x400000L)) dumpAt(va);
        header("OTNT -- priority mask 0x2000000");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x2000000L)) dumpAt(va);
        header("OTNT -- priority mask 0x4000000");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x4000000L)) dumpAt(va);
        header("OTNT -- capability mask 0x20");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x20L)) dumpAt(va);
        header("OTNT -- capability mask 0x100");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x100L)) dumpAt(va);
        header("OTNT -- capability mask 0x200");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x200L)) dumpAt(va);
        header("OTNT -- capability mask 0x400");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x400L)) dumpAt(va);
        header("OTNT -- capability mask 0x800");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x800L)) dumpAt(va);
        header("OTNT -- hardpoint $2 scan 0x460000-0x480000");
        for (long va : findFunctionsWithMask(0x00460000L, 0x00480000L, 0x2L)) dumpAt(va);
        header("OTNT -- ot_flags bit 17 (0x20000) scan");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x20000L)) dumpAt(va);
        header("OTNT -- ot_flags bit 21 (0x200000) scan");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x200000L)) dumpAt(va);
        header("OTNT -- NT flag bits 18-20 scan GV-range 0x470000-0x480000");
        for (long m : new long[]{0x40000L, 0x80000L, 0x100000L})
            for (long va : findFunctionsWithMask(0x00470000L, 0x00480000L, m)) dumpAt(va);
        header("OTNT -- NT flag bits 25-26 scan GV-range");
        for (long m : new long[]{0x2000000L, 0x4000000L})
            for (long va : findFunctionsWithMask(0x00470000L, 0x00480000L, m)) dumpAt(va);
        header("OTNT -- NT bit 10 (0x400) narrowed to 0x470000-0x480000");
        for (long va : findFunctionsWithMask(0x00470000L, 0x00480000L, 0x400L)) dumpAt(va);
        header("OTNT -- symbols matching ot/nt/gv/vehicle/ship/naval/spawn/capability/hardfire");
        dumpSymbolsMatching("gvproc", "gv", "vehicle", "ship", "naval", "spawn",
                "hardfire", "@hardfire", "otnt", "ot_", "nt_", "civilian", "dual");
    }

    // -----------------------------------------------------------------------
    // T2 -- terrain tile system
    // Consolidates: DumpT2Loader, DumpT2MMCoords
    // -----------------------------------------------------------------------
    protected void analyzeT2() throws Exception {
        header("T2 -- @G_Tile@32 (0x447aa5)");
        dumpAt(0x00447aa5L);
        header("T2 -- _G_TileInit (0x447a40)");
        dumpAt(0x00447a40L);
        header("T2 -- do_use_terrain_detail (0x4d2344)");
        dumpAt(0x004d2344L);
        header("T2 -- callers of @G_Tile@32");
        dumpCallers(0x00447aa5L);
        header("T2 -- tile cluster 0x447a00-0x447f00");
        dumpRange(0x00447a00L, 0x00447f00L);
        header("T2 -- ?MAPWorldToScreen (0x422380)");
        dumpAt(0x00422380L);
        header("T2 -- _GetGround@0 (0x47af70)");
        dumpAt(0x0047af70L);
        header("T2 -- MM/lib area 0x479e00-0x47a600");
        dumpRange(0x00479e00L, 0x0047a600L);
        header("T2 -- T2 sub-header constant scan (0x95, 0x80, 195, 21)");
        for (long c : new long[]{0x95L, 0x80L, 195L, 21L})
            for (long va : findFunctionsReadingOffsets(0x00400000L, 0x00600000L, (int)c, (int)c))
                dumpAt(va);
        header("T2 -- string search: BIT2 magic / .T2 / tmap / tdic");
        searchStrings(new String[]{"BIT2", ".T2", "tmap", "tdic", "textFormat"});
        header("T2 -- symbols matching t2/terrain/tile/tmap/tdic/mapworld");
        dumpSymbolsMatching("t2", "terrain", "tile", "tmap", "tdic", "mapworld",
                "getground", "worldtoscreen");
        header("T2 -- warhead/hit/fuse symbols");
        dumpSymbolsMatching("warhead", "fuse", "arm", "detonate", "explode", "hit", "prox");
    }

    // -----------------------------------------------------------------------
    // GAS -- fuel, hardpoints, BRF, vehicle weapons
    // Consolidates: DumpBRFFunctions, DumpGASFuel, DumpGVProcGAS,
    //               DumpGVProcHandlers
    // -----------------------------------------------------------------------
    protected void analyzeGAS() throws Exception {
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
    }

    // -----------------------------------------------------------------------
    // CAM -- campaign DLL binary layout (FA.EXE side)
    // -----------------------------------------------------------------------
    protected void analyzeCAM() throws Exception {
        header("CAM -- campaign launcher FUN_00428412");
        dumpAtForced(0x00428412L);
        header("CAM -- callers of campaign launcher");
        dumpCallers(0x00428412L);
        header("CAM -- MC loader (0x481940)");
        dumpAt(0x00481940L);
        header("CAM -- callers of MC loader");
        dumpCallers(0x00481940L);
        header("CAM -- _MISSIONInit2 (0x480b50)");
        dumpAt(0x00480b50L);
        header("CAM -- _ChooseActivity (0x4a08a0)");
        dumpAt(0x004a08a0L);
        header("CAM -- callers of _ChooseActivity");
        dumpCallers(0x004a08a0L);
        header("CAM -- campaign range 0x428000-0x430000");
        dumpRange(0x00428000L, 0x00430000L);
        header("CAM -- asset loader area 0x4a6cc0-0x4a7200");
        dumpRange(0x004a6cc0L, 0x004a7200L);
        header("CAM -- string search: campaign / theater keywords");
        searchStrings(new String[]{".CAM", ".cam", "BALTIC", "PERSIAN", "KOREA",
                "CHINA", "EGYPT", "VIETNAM", "campaign", "theater", "MISSION", "mission"});
        header("CAM -- symbols matching cam/campaign/theater/mission/loadout/mc");
        dumpSymbolsMatching("cam", "campaign", "theater", "mission", "loadout",
                "mcload", "mcproc", "mceval", "choosecampaign", "choosemission");
    }

    // -----------------------------------------------------------------------
    // MC -- mission condition DLL flow
    // -----------------------------------------------------------------------
    protected void analyzeMC() throws Exception {
        header("MC -- MC loader (0x481940)");
        dumpAt(0x00481940L);
        header("MC -- callers of MC loader");
        dumpCallers(0x00481940L);
        header("MC -- _MISSIONInit2 (0x480b50)");
        dumpAt(0x00480b50L);
        header("MC -- callers of _MISSIONInit2");
        dumpCallers(0x00480b50L);
        header("MC -- MM line parser FUN_0047a130");
        dumpAt(0x0047a130L);
        header("MC -- MM keyword handler FUN_0047a510");
        dumpAt(0x0047a510L);
        header("MC -- mission range 0x480000-0x483000");
        dumpRange(0x00480000L, 0x00483000L);
        header("MC -- mission support 0x483000-0x486000");
        dumpRange(0x00483000L, 0x00486000L);
        header("MC -- object state area 0x473000-0x476000");
        dumpRange(0x00473000L, 0x00476000L);
        header("MC -- trigger/event range 0x486000-0x489000");
        dumpRange(0x00486000L, 0x00489000L);
        header("MC -- string search: MC condition keywords");
        searchStrings(new String[]{".MC", "COND", "cond", "TRIGGER", "trigger",
                "DESTROY", "destroy", "CAPTURE", "capture", "SUCCEED", "FAIL",
                "mczonecheck", "mccheck", "mcevaluate"});
        header("MC -- symbols matching mc/mission/cond/trigger/zone/objective/succeed/fail");
        dumpSymbolsMatching("mc", "mission", "cond", "trigger", "zone",
                "objective", "succeed", "fail", "mcinit", "mcproc", "mceval",
                "mccheck", "mczonecheck");
    }

    // -----------------------------------------------------------------------
    // T2DLL -- T2 terrain overlay DLL / surface-class mapping
    // -----------------------------------------------------------------------
    protected void analyzeT2DLL() throws Exception {
        header("T2DLL -- do_use_terrain_detail (0x4d2344)");
        dumpAt(0x004d2344L);
        header("T2DLL -- callers of do_use_terrain_detail");
        dumpCallers(0x004d2344L);
        header("T2DLL -- @G_Tile@32 (0x447aa5)");
        dumpAt(0x00447aa5L);
        header("T2DLL -- tileExpand (0x4f4f78)");
        dumpAt(0x004f4f78L);
        header("T2DLL -- callers of tileExpand");
        dumpCallers(0x004f4f78L);
        header("T2DLL -- expandTerrain (0x50e145)");
        dumpAt(0x0050e145L);
        header("T2DLL -- callers of expandTerrain");
        dumpCallers(0x0050e145L);
        header("T2DLL -- T2 DLL load area 0x4d2000-0x4d5000");
        dumpRange(0x004d2000L, 0x004d5000L);
        header("T2DLL -- tileExpand area 0x4f4000-0x4f6000");
        dumpRange(0x004f4000L, 0x004f6000L);
        header("T2DLL -- expandTerrain area 0x50e000-0x510000");
        dumpRange(0x0050e000L, 0x00510000L);
        header("T2DLL -- sub-header constant 0x95 scan 0x4d0000-0x510000");
        for (long va : findFunctionsWithMask(0x004d0000L, 0x00510000L, 0x95L)) dumpAt(va);
        header("T2DLL -- sub-header constant 0xC3 scan");
        for (long va : findFunctionsWithMask(0x004d0000L, 0x00510000L, 0xC3L)) dumpAt(va);
        header("T2DLL -- surface-class offset scan (0-0x20) in T2 area");
        for (long va : findFunctionsReadingOffsets(0x004d0000L, 0x00510000L, 0, 0x20)) dumpAt(va);
        header("T2DLL -- string search: BIT2 / T2 terrain keywords");
        searchStrings(new String[]{"BIT2", ".T2", ".t2", "tmap", "tdic",
                "terrain", "TERRAIN", "surface", "tiletype", "tileclass"});
        header("T2DLL -- _GetGround@0 (0x47af70)");
        dumpAt(0x0047af70L);
        header("T2DLL -- callers of _GetGround@0");
        dumpCallers(0x0047af70L);
        header("T2DLL -- ?MAPWorldToScreen (0x422380)");
        dumpAt(0x00422380L);
        header("T2DLL -- symbols matching t2/terrain/tile/surface/ground/bit2/expand");
        dumpSymbolsMatching("t2", "terrain", "tile", "surface", "ground",
                "bit2", "expand", "tilemap", "tileclass", "terrainload",
                "t2init", "t2load", "t2parse");
    }

    // -----------------------------------------------------------------------
    // GAMELOOP -- main game loop, per-frame object update, initialization
    // -----------------------------------------------------------------------
    protected void analyzeGameLoop() throws Exception {
        header("INIT -- WinMain / _main entry point");
        dumpSymbolsMatching("winmain", "_main", "maincrtstartup", "wwinmain");
        searchStrings(new String[]{"WinMain", "GameLoop", "JANES", "Fighters Anthology"});
        header("INIT -- game initialization sequence");
        dumpSymbolsMatching("initgame", "_initgame", "gameinit", "_gameinit", "init_game",
                "startgame", "_startgame", "gamestart", "_gamestart");
        header("LOOP -- main game loop body");
        dumpSymbolsMatching("gameloop", "_gameloop", "mainloop", "_mainloop",
                "frameupdate", "_frameupdate", "gametick", "_gametick");
        header("LOOP -- frame counter and timing");
        dumpSymbolsMatching("framecounter", "frametimer", "gametimer", "_gametimer",
                "timestep", "_timestep", "tickcount", "_tickcount");
        searchStrings(new String[]{"fps", "FPS", "frame"});
        header("LOOP -- per-frame object iterator / dispatcher 0x468000-0x479DFF");
        dumpRange(0x00468000L, 0x00479DFFL);
        header("OBJ -- object add / remove / iterate");
        dumpSymbolsMatching("_objadd", "objadd", "_objremove", "objremove",
                "_objinit", "objinit", "_objproc", "_objupdate", "objupdate",
                "_objlist", "objlist", "_entityadd", "entityadd");
        header("OBJ -- callers of _GVProc (0x473db0)");
        dumpCallers(0x00473db0L);
        header("OBJ -- callers of _PROJProc (0x4c1f50)");
        dumpCallers(0x004c1f50L);
        header("INIT -- range 0x401000-0x406000 (entry point cluster)");
        dumpRange(0x00401000L, 0x00406000L);
        header("INIT -- campaign and mission init");
        dumpSymbolsMatching("_missioninit", "missioninit", "_campaigninit", "campaigninit",
                "_missionstart", "missionstart", "_levelload", "levelload");
        header("SHUTDOWN -- game shutdown and cleanup");
        dumpSymbolsMatching("shutdown", "_shutdown", "gameshutdown", "_gameshutdown",
                "cleanup", "_cleanup", "exitgame", "_exitgame");
    }

    // -----------------------------------------------------------------------
    // RENDERER -- 3D rendering pipeline, shape/sprite system, camera
    // -----------------------------------------------------------------------
    protected void analyzeRenderer() throws Exception {
        header("RENDER -- scene dispatch and render loop");
        dumpSymbolsMatching("renderscene", "_renderscene", "drawscene", "_drawscene",
                "rendworld", "_rendworld", "renderframe", "_renderframe",
                "drawworld", "_drawworld", "renderall", "_renderall");
        header("RENDER -- callers of T_DefaultHorizon (0x4aacf0)");
        dumpCallers(0x004aacf0L);
        header("RENDER -- callers of @G_Tile@32 (0x447aa5)");
        dumpCallers(0x00447aa5L);
        header("SHAPE -- shape load and cache");
        dumpSymbolsMatching("_shload", "shload", "_loadsh", "loadsh", "_shapeinit", "shapeinit",
                "_shrender", "shrender", "_drawsh", "drawsh", "_rendersh", "rendersh",
                "_shcache", "shcache", "_shfree", "shfree");
        searchStrings(new String[]{".SH", ".sh", "wave1", "SH\0"});
        header("SHAPE -- shape manager range 0x4B4200-0x4BEDFF");
        dumpRange(0x004b4200L, 0x004bedffL);
        header("POLY -- polygon submission and rasterizer");
        dumpSymbolsMatching("_polygon", "polygon", "_drawpoly", "drawpoly", "_renderpoly",
                "renderpoly", "_vertex", "vertex", "_triangle", "triangle",
                "_zbuffer", "zbuffer", "_zbuf", "zbuf", "_zwrite", "zwrite");
        header("SPRITE -- sprite and billboard rendering");
        dumpSymbolsMatching("_sprite", "sprite", "_billboard", "billboard",
                "_drawsprite", "drawsprite", "_renderobj2d", "renderobj2d",
                "_particle", "particle", "_explosion", "explosion");
        header("CAMERA -- viewport and projection");
        dumpSymbolsMatching("_camera", "camera", "_viewport", "viewport",
                "_projection", "projection", "_frustum", "frustum",
                "_setcamera", "setcamera", "_lookat", "lookat",
                "_camupdate", "camupdate", "_camfollow", "camfollow");
        searchStrings(new String[]{"camera", "Camera", "viewport", "Viewport"});
        header("CULL -- visibility culling and spatial partitioning");
        dumpSymbolsMatching("_cull", "cull", "_visible", "visible", "_frustumcull",
                "frustumcull", "_occlude", "occlude", "_lod", "lod",
                "_inview", "inview", "_isvisible", "isvisible");
        header("DDRAW -- DirectDraw surface management");
        dumpSymbolsMatching("_ddraw", "ddraw", "_surface", "surface", "_flip", "flip",
                "_blit", "blit", "_vga", "vga", "_screen", "screen",
                "_pageflip", "pageflip", "_backbuffer", "backbuffer");
        searchStrings(new String[]{"DirectDraw", "IDirectDraw", "CreateSurface"});
        header("WR -- WR raster rendering subsystem");
        dumpSymbolsMatching("_wr", "wrsetup", "wrinit", "wrflush", "wrrender",
                "_wrsetremaps", "wrsetremaps", "_wrpoly", "wrpoly");
        header("PIC -- texture/PIC loading for renderer");
        dumpSymbolsMatching("_picload", "picload", "_loadpic", "loadpic",
                "_picrender", "picrender", "_pictexture", "pictexture");
        searchStrings(new String[]{".PIC", ".pic"});
    }

    // -----------------------------------------------------------------------
    // PHYSICS -- flight model, aerodynamics, collision detection
    // -----------------------------------------------------------------------
    protected void analyzePhysics() throws Exception {
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
    }

    // -----------------------------------------------------------------------
    // NETWORK -- multiplayer protocol, CN_INFO struct, transport layer
    // -----------------------------------------------------------------------
    protected void analyzeNetwork() throws Exception {
        header("CN -- CN_ReadConfig / CN_WriteConfig (CN_INFO struct)");
        dumpSymbolsMatching("cn_readconfig", "_cn_readconfig", "cn_writeconfig", "_cn_writeconfig",
                "cnreadconfig", "cnwriteconfig", "cn_init", "_cn_init", "cninit");
        header("CN -- CN_INFO struct field scan (offsets 0x00-0x200)");
        findFunctionsReadingOffsets(0x00400000L, 0x00540000L, 0x00, 0x200);
        header("MP -- ?MPReceive@@YGDXZ (0x46C980) and callers");
        dumpAt(0x0046c980L);
        dumpCallers(0x0046c980L);
        header("MP -- multiplayer state and symbols");
        dumpSymbolsMatching("mpreceive", "mpsend", "mpupdate", "mpinit", "mpshutdown",
                "mpframe", "mpsync", "mpstatus", "mpsession",
                "mpjoin", "mphost", "mpstart", "mpend", "mpkill",
                "?mpset", "?mpget", "?mpstatus", "?mprecv", "?mpsend",
                "_mprecv", "_mpsend", "_mpupdate", "_mpinit");
        header("MP -- range 0x482200-0x4AACEF");
        dumpRange(0x00482200L, 0x004aacefL);
        header("NET -- IPX transport");
        dumpSymbolsMatching("_ipxsend", "ipxsend", "_ipxrecv", "ipxrecv",
                "_ipxinit", "ipxinit", "_ipxopen", "ipxopen",
                "_ipxclose", "ipxclose", "_ipxpoll", "ipxpoll");
        searchStrings(new String[]{"IPX", "ipx", "Novell", "novell"});
        header("NET -- TCP/IP transport");
        dumpSymbolsMatching("_tcpsend", "tcpsend", "_tcprecv", "tcprecv",
                "_tcpinit", "tcpinit", "_tcpopen", "tcpopen",
                "_tcpclose", "tcpclose", "_tcpconnect", "tcpconnect");
        searchStrings(new String[]{"TCP", "tcp", "socket", "Socket", "winsock", "WinSock"});
        header("NET -- serial / modem transport");
        dumpSymbolsMatching("_serialsend", "serialsend", "_serialrecv", "serialrecv",
                "_modemsend", "modemsend", "_modemrecv", "modemrecv",
                "_comminit", "comminit", "_commopen", "commopen",
                "_commsend", "commsend", "_commrecv", "commrecv");
        searchStrings(new String[]{"COM", "modem", "Modem", "serial", "Serial"});
        header("NET -- packet encode / decode");
        dumpSymbolsMatching("_packetinit", "packetinit", "_packencode", "packencode",
                "_packdecode", "packdecode", "_packetsend", "packetsend",
                "_packetrecv", "packetrecv", "_netpacket", "netpacket");
        header("NET -- session management");
        dumpSymbolsMatching("_netsession", "netsession", "_sessioninit", "sessioninit",
                "_sessionjoin", "sessionjoin", "_sessionhost", "sessionhost",
                "_sessionlist", "sessionlist", "_sessionclose", "sessionclose");
        searchStrings(new String[]{"session", "Session", "lobby", "Lobby", "host", "join"});
        header("MP -- player/entity sync over network");
        dumpSymbolsMatching("mpsetfuel", "?mpsetfuel", "mpsetpos", "mpsetstate",
                "mpsetweapon", "mpsetdamage", "mpentityupdate", "mpplayerupdate");
    }

    // -----------------------------------------------------------------------
    // INPUT -- joystick, keyboard, mouse, world-coordinate transforms
    // -----------------------------------------------------------------------
    protected void analyzeInput() throws Exception {
        header("INPUT -- joystick init and poll");
        dumpSymbolsMatching("_joyinit", "joyinit", "_joypoll", "joypoll",
                "_joyread", "joyread", "_joyupdate", "joyupdate",
                "_joyopen", "joyopen", "_joyclose", "joyclose",
                "_joystick", "joystick", "_joycal", "joycal",
                "_joyaxis", "joyaxis", "_joybutton", "joybutton",
                "_dinputinit", "dinputinit", "_dinpoll", "dinpoll");
        searchStrings(new String[]{"joystick", "Joystick", "DirectInput", "IDirectInput",
                "joyGetDevCaps", "joyGetPos", "joyGetPosEx"});
        header("INPUT -- range 0x420000-0x42FFFF");
        dumpRange(0x00420000L, 0x0042ffffL);
        header("INPUT -- keyboard handling");
        dumpSymbolsMatching("_keyinit", "keyinit", "_keypoll", "keypoll",
                "_keyread", "keyread", "_keydown", "keydown", "_keyup", "keyup",
                "_keyboard", "keyboard", "_keybind", "keybind",
                "_hotkey", "hotkey", "_keymap", "keymap");
        searchStrings(new String[]{"GetAsyncKeyState", "GetKeyState", "keyboard", "Keyboard"});
        header("INPUT -- mouse handling");
        dumpSymbolsMatching("_mouseinit", "mouseinit", "_mousepoll", "mousepoll",
                "_mouseread", "mouseread", "_mousepos", "mousepos",
                "_mousebutton", "mousebutton", "_mousemove", "mousemove",
                "_cursorinit", "cursorinit", "_cursor", "cursor");
        searchStrings(new String[]{"GetCursorPos", "SetCursorPos", "ShowCursor",
                "mouse", "Mouse"});
        header("INPUT -- control mapping and deadzone");
        dumpSymbolsMatching("_ctrlmap", "ctrlmap", "_controlmap", "controlmap",
                "_deadzone", "deadzone", "_sensitivity", "sensitivity",
                "_calibrate", "calibrate", "_ctrlupdate", "ctrlupdate",
                "_inputmap", "inputmap", "_axisinvert", "axisinvert");
        header("WORLD -- ?MAPWorldToScreen (0x422380)");
        dumpAt(0x00422380L);
        header("WORLD -- world coordinate symbols");
        dumpSymbolsMatching("worldtoscreen", "screentoworld", "worldtovp",
                "mapworld", "_mapworld", "_worldcoord", "worldcoord",
                "_wtoscreen", "wtoscreen", "_screenproject", "screenproject");
        header("WORLD -- viewport and projection symbols");
        dumpSymbolsMatching("_vpinit", "vpinit", "_vpupdate", "vpupdate",
                "_project", "projectpoint", "_unproject", "unproject",
                "_screentransform", "screentransform");
    }

    // -----------------------------------------------------------------------
    // BIFRAME -- BI opcode 0x28 (FRAME) consumer search
    // The FRAME opcode writes two s16 values to the CT interpreter state block:
    //   DAT_00546c44 (+0x7c) and DAT_00546c46 (+0x7e).
    // The reader accesses these through a struct pointer, not by direct address.
    // -----------------------------------------------------------------------
    protected void analyzeBIFRAME() throws Exception {
        header("BIFRAME -- FRAME writer site: dispatch area 0x4ace00-0x4ad800");
        dumpRange(0x004ace00L, 0x004ad800L);
        header("BIFRAME -- CT state save FUN_004668f0");
        dumpAt(0x004668f0L);
        header("BIFRAME -- CT state restore FUN_00466920");
        dumpAt(0x00466920L);
        header("BIFRAME -- findFunctionsReadingOffsets +0x7c (FRAME s16 in CT state block)");
        for (long va : findFunctionsReadingOffsets(0x00400000L, 0x00540000L, 0x7c, 0x80)) dumpAt(va);
        header("BIFRAME -- _INFO2Draw candidate");
        dumpSymbolsMatching("INFO2Draw", "info2draw", "Info2Draw");
        header("BIFRAME -- _FMFlight@0 (0x47b020) candidate consumer");
        dumpAtForced(0x0047b020L);
        header("BIFRAME -- _MANAdd@24 (0x47ceb0) candidate consumer");
        dumpAtForced(0x0047ceb0L);
        header("BIFRAME -- _GVDoCurrentWaypoint / MPStatusSet (symbol search)");
        dumpSymbolsMatching("GVDoCurrentWaypoint", "gvdocurrentwaypoint",
                "MPStatusSet", "mpstatusset");
        header("BIFRAME -- FUN_0048e740 candidate consumer");
        dumpAtForced(0x0048e740L);
        header("BIFRAME -- direct-address callers of DAT_00546c44 / DAT_00546c46");
        dumpCallers(0x00546c44L);
        dumpCallers(0x00546c46L);
        header("BIFRAME -- callers of DAT_0050cf90 (CT block saved-copy pointer)");
        dumpCallers(0x0050cf90L);
    }
}
