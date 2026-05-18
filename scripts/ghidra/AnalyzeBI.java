// Consolidates: DumpAIScripts, DumpCTOpcodeArgs, DumpPlaneCheckFuelCT

public class AnalyzeBI extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeBI");

        // BI bytecode interpreter
        header("_CTExecProgram@4 (0x466970)");
        dumpAt(0x00466970L);

        header("Callers of _CTExecProgram@4");
        dumpCallers(0x00466970L);

        header("Functions near _CTExecProgram (0x466800-0x466a00)");
        dumpRange(0x00466800L, 0x00466a00L);

        header("Opcode dispatcher FUN_00466a80");
        dumpAtForced(0x00466a80L);

        // CTDo_ handlers
        header("_CTDo_move (0x465cc0)");
        dumpAtForced(0x00465cc0L);

        header("_CTDo_movetoalt (0x465e20)");
        dumpAtForced(0x00465e20L);

        header("_CTDo_jink (0x4663f0)");
        dumpAtForced(0x004663f0L);

        header("_CTDo_maneuver (0x465a70)");
        dumpAtForced(0x00465a70L);

        header("_CTDo_turn (0x465ea0)");
        dumpAtForced(0x00465ea0L);

        // Arg reader cluster
        header("Arg reader cluster 0x465c00-0x465f00");
        dumpRange(0x00465c00L, 0x00465f00L);
        dumpAtForced(0x00465c90L);
        dumpAtForced(0x00465d40L);
        dumpAtForced(0x00465da0L);
        dumpAtForced(0x00465de0L);
        dumpAtForced(0x00465e00L);

        // CTEval_ handlers
        header("_CTEval_ir (0x4650e0)");
        dumpAt(0x004650e0L);

        header("_CTEval_radar (0x4650a0)");
        dumpAt(0x004650a0L);

        header("_CTEval_do_ir_launch (0x464e70)");
        dumpAt(0x00464e70L);

        header("_CTEval_do_radar_launch (0x464e60)");
        dumpAt(0x00464e60L);

        // Movement execution
        header("_MVRJink@40 (0x4ac9e0)");
        dumpAt(0x004ac9e0L);

        header("_MVRMove (0x4ac510)");
        dumpAt(0x004ac510L);

        // Command buffer writers
        header("_CreateMove (0x463a20)");
        dumpAt(0x00463a20L);

        header("_CreateMoveGoal (0x463af0)");
        dumpAt(0x00463af0L);

        header("@WriteCmdBufMove (0x463cc0)");
        dumpAt(0x00463cc0L);

        // Mission init (BI context)
        header("_MISSIONInit2 (0x480b50)");
        dumpAt(0x00480b50L);

        header("Callers of _MISSIONInit2");
        dumpCallers(0x00480b50L);

        // All CTDo_ via symbol table
        header("All _CTDo_* symbols");
        dumpSymbolsMatching("ctdo_", "cteval_", "ctexec", "cttry", "ctplan");

        // FRAME opcode 0x28 — writes two s16 values to DAT_00546c44 and DAT_00546c46.
        // Every bytecode dispatch entry is prefixed with FRAME. The first s16 is a block
        // ID (1–6 reserved, dispatch starts at 7); the second is monotonically increasing.
        // The consumer of these globals has not been found; scan a wide range for reads.
        header("FRAME opcode reader: xrefs to DAT_00546c44");
        dumpXrefsToData(0x00546c44L);

        header("FRAME opcode reader: xrefs to DAT_00546c46");
        dumpXrefsToData(0x00546c46L);

        // Widen scan to find profiling/priority subsystem that reads these globals
        header("Wide scan for reads of DAT_00546c44 address range 0x460000-0x490000");
        for (long va : findFunctionsReadingOffsets(0x00460000L, 0x00490000L, 0x546c44 & 0xFF, 0x546c46 & 0xFF))
            dumpAt(va);

        // Bytecode interpreter state globals — IP, priority, actor, halt
        header("Xrefs to BI runtime state DAT_00546bea (IP)");
        dumpXrefsToData(0x00546beaL);

        header("Xrefs to BI runtime state DAT_00546bf0 (priority)");
        dumpXrefsToData(0x00546bf0L);

        header("Xrefs to BI actor DAT_00546c94");
        dumpXrefsToData(0x00546c94L);

        header("Xrefs to BI halt flag DAT_00546c98");
        dumpXrefsToData(0x00546c98L);

        closeOutput();
    }

    private void dumpXrefsToData(long va) throws Exception {
        ghidra.program.model.address.Address addr = toAddr(va);
        for (ghidra.program.model.symbol.Reference ref :
                currentProgram.getReferenceManager().getReferencesTo(addr)) {
            ghidra.program.model.listing.Function fn =
                    currentProgram.getFunctionManager().getFunctionContaining(ref.getFromAddress());
            if (fn != null) dumpAt(fn.getEntryPoint().getOffset());
        }
    }
    }
}
