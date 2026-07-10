// Consolidates: DumpAIScripts, DumpCTOpcodeArgs, DumpPlaneCheckFuelCT

public class AnalyzeBI extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeBI");
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
        closeOutput();
    }
}
