// Master FA.EXE analysis script.
// Runs every subsystem analysis in sequence into a single output file.
// Invoke: run_ghidra.bat AnalyzeFA.java
// Output: %FA_PROJECT%\output\AnalyzeFA.txt

public class AnalyzeFA extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeFA");

        analyzeLAY();
        analyzeHUD();
        analyzeDLG();
        analyzePROJ();
        analyzeSEE();
        analyzeMM();
        analyzeBI();
        analyzeECM();
        analyzeHGR();
        analyzeMUS();
        analyzeOTNT();
        analyzeT2();
        analyzeGAS();

        closeOutput();
    }

    // -----------------------------------------------------------------------
    // LAY â€” sky / atmosphere layer system
    // -----------------------------------------------------------------------
    private void analyzeLAY() throws Exception {
        header("LAY â€” ParseLayerFile (0x4b4370)");
        dumpAt(0x004b4370L);
        header("LAY â€” CopyLayersToRuntime (0x4b3750)");
        dumpAt(0x004b3750L);
        header("LAY â€” InterpolateLayers (0x4b3820)");
        dumpAt(0x004b3820L);
        header("LAY â€” GetLayerAtAltitude (0x4b3be0)");
        dumpAt(0x004b3be0L);
        header("LAY â€” T_DefaultHorizon (0x4aacf0)");
        dumpAt(0x004aacf0L);
        header("LAY â€” ApplyBrightnessGradient (0x4b3cb0)");
        dumpAt(0x004b3cb0L);
        header("LAY â€” UpdateSkyState (0x4b3d90)");
        dumpAt(0x004b3d90L);
        header("LAY â€” UpdateAuroraClouds (0x4b4170)");
        dumpAt(0x004b4170L);
        header("LAY â€” FindNearestColorEntry (0x4b3ad0)");
        dumpAt(0x004b3ad0L);
        header("LAY â€” LoadPICByWildcard (0x4b4680)");
        dumpAt(0x004b4680L);
        header("LAY â€” GetLayerBoundary (0x4b3190)");
        dumpAt(0x004b3190L);
        header("LAY â€” GetLayerByIndex (0x4b3170)");
        dumpAt(0x004b3170L);
        header("LAY â€” WRFogLayerUpdate (0x4b4320)");
        dumpAt(0x004b4320L);
        header("LAY â€” all callers of ParseLayerFile");
        dumpCallers(0x004b4370L);
        header("LAY â€” all callers of WRFogLayerUpdate");
        dumpCallers(0x004b4320L);
        header("LAY â€” range 0x4b2ea0-0x4b4200");
        dumpRange(0x004b2ea0L, 0x004b4200L);
    }

    // -----------------------------------------------------------------------
    // HUD â€” heads-up display
    // -----------------------------------------------------------------------
    private void analyzeHUD() throws Exception {
        header("HUD â€” draw dispatcher / init (0x406040)");
        dumpAt(0x00406040L);
        header("HUD â€” warning lights FUN_00407930");
        dumpAt(0x00407930L);
        header("HUD â€” FUN_00407ee0");
        dumpAt(0x00407ee0L);
        header("HUD â€” FUN_00408420");
        dumpAt(0x00408420L);
        header("HUD â€” FUN_00407a00");
        dumpAt(0x00407a00L);
        header("HUD â€” FUN_00416380");
        dumpAt(0x00416380L);
        header("HUD â€” _PLANECheckFuel@0 (0x49fb70)");
        dumpAt(0x0049fb70L);
        header("HUD â€” _PROJProc (0x4c1f50)");
        dumpAt(0x004c1f50L);
        header("HUD â€” PROJMoveProc (0x4c11b0)");
        dumpAt(0x004c11b0L);
        header("HUD â€” callers of _HARDPtrs@12 (0x452770)");
        dumpCallers(0x00452770L);
        header("HUD â€” draw range 0x407b60-0x40ac00");
        dumpRange(0x00407b60L, 0x0040ac00L);
        header("HUD â€” 0x4000 (bit14) writers 0x400000-0x500000");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00500000L, 0x4000L)) dumpAt(va);
    }

    // -----------------------------------------------------------------------
    // DLG â€” dialog / UI system
    // -----------------------------------------------------------------------
    private void analyzeDLG() throws Exception {
        header("DLG â€” _DialogWhatItem (0x488fc0)");
        dumpAtForced(0x00488fc0L);
        header("DLG â€” _DialogShow (0x4880d0)");
        dumpAt(0x004880d0L);
        header("DLG â€” _DialogShutDown (0x488190)");
        dumpAt(0x00488190L);
        header("DLG â€” _DialogDone (0x488300)");
        dumpAt(0x00488300L);
        header("DLG â€” @DialogGetPtr (0x4892e0)");
        dumpAt(0x004892e0L);
        header("DLG â€” @DialogGetValue (0x489300)");
        dumpAt(0x00489300L);
        header("DLG â€” @DialogSelectItem (0x4894f0)");
        dumpAt(0x004894f0L);
        header("DLG â€” @DialogDeselectItem (0x489580)");
        dumpAtForced(0x00489580L);
        header("DLG â€” _ChoosePreload (0x4897f0)");
        dumpAt(0x004897f0L);
        header("DLG â€” _ChooseActivity (0x4a08a0)");
        dumpAt(0x004a08a0L);
        header("DLG â€” FUN_004a6e20 (dispatcher)");
        dumpAt(0x004a6e20L);
        header("DLG â€” draw: _DrawText/Action/FormattedText/CampaignList/Rocker/EditBox");
        dumpAt(0x00489ac0L); dumpAt(0x00489b90L); dumpAt(0x0048a910L);
        dumpAt(0x0048abf0L); dumpAt(0x0048b4e0L); dumpAt(0x0048c710L);
        header("DLG â€” callers of _ChoosePreload");
        dumpCallers(0x004897f0L);
        header("DLG â€” callers of dispatcher");
        dumpCallers(0x004a6e20L);
    }

    // -----------------------------------------------------------------------
    // PROJ â€” projectile / missile system
    // -----------------------------------------------------------------------
    private void analyzePROJ() throws Exception {
        header("PROJ â€” _PROJInit@0 (0x4c06a0)");
        dumpAt(0x004c06a0L);
        header("PROJ â€” _PROJAdd@40 (0x4c0a90)");
        dumpAt(0x004c0a90L);
        header("PROJ â€” _PROJFire@16 (0x4c2170)");
        dumpAt(0x004c2170L);
        header("PROJ â€” _PROJProc (0x4c1f50)");
        dumpAt(0x004c1f50L);
        header("PROJ â€” ?PROJMoveProc (0x4c11b0)");
        dumpAt(0x004c11b0L);
        header("PROJ â€” _PROJSpeed@8 (0x4c1120)");
        dumpAt(0x004c1120L);
        header("PROJ â€” _PROJEngineState@0 (0x4c1170)");
        dumpAt(0x004c1170L);
        header("PROJ â€” _PROJLockUpdate@0 (0x4c0960)");
        dumpAt(0x004c0960L);
        header("PROJ â€” _PROJHitChance@28 (0x4c3380)");
        dumpAt(0x004c3380L);
        header("PROJ â€” _PROJLock@24 (0x4c2f20)");
        dumpAt(0x004c2f20L);
        header("PROJ â€” _DAMAGEDoHit (0x40f970)");
        dumpAt(0x0040f970L);
        header("PROJ â€” proximity fuze (0x4c3960)");
        dumpAt(0x004c3960L);
        header("PROJ â€” offset scan 0x50-0x7f");
        for (long va : findFunctionsReadingOffsets(0x00400000L, 0x00500000L, 0x50, 0x7F)) dumpAt(va);
        header("PROJ â€” cluster 0x4c0000-0x4c3000");
        dumpRange(0x004c0000L, 0x004c3000L);
    }

    // -----------------------------------------------------------------------
    // SEE â€” seeker / missile guidance
    // -----------------------------------------------------------------------
    private void analyzeSEE() throws Exception {
        header("SEE â€” seeker cone / _PROJInFOV (0x4c2860)");
        dumpAt(0x004c2860L);
        header("SEE â€” search lobe (0x4c2eb0)");
        dumpAt(0x004c2eb0L);
        header("SEE â€” track lobe (0x4c31f0)");
        dumpAt(0x004c31f0L);
        header("SEE â€” pre-lobe (0x4c24b0)");
        dumpAt(0x004c24b0L);
        header("SEE â€” guidance final (0x4c26f0)");
        dumpAt(0x004c26f0L);
        header("SEE â€” lock-acquire 1 (0x4c5000)");
        dumpAt(0x004c5000L);
        header("SEE â€” lock-acquire 2 (0x4c5050)");
        dumpAt(0x004c5050L);
        header("SEE â€” target selection (0x4c52d0)");
        dumpAt(0x004c52d0L);
        header("SEE â€” missile flag handler (0x4c3eb0)");
        dumpAt(0x004c3eb0L);
        header("SEE â€” re-eval (0x4c4100)");
        dumpAt(0x004c4100L);
        header("SEE â€” wrapper (0x4c58a0)");
        dumpAt(0x004c58a0L);
        header("SEE â€” callers of seeker cone");
        dumpCallers(0x004c2860L);
        header("SEE â€” 0x20000 flag scan 0x4c0000-0x4c7000");
        for (long va : findFunctionsWithMask(0x004c0000L, 0x004c7000L, 0x20000L)) dumpAt(va);
        header("SEE â€” 0x100000 flag scan");
        for (long va : findFunctionsWithMask(0x004c0000L, 0x004c7000L, 0x100000L)) dumpAt(va);
    }

    // -----------------------------------------------------------------------
    // MM â€” mission map / campaign
    // -----------------------------------------------------------------------
    private void analyzeMM() throws Exception {
        header("MM â€” keyword handler FUN_0047a510");
        dumpAt(0x0047a510L);
        header("MM â€” FUN_0047a130 (line parser)");
        dumpAt(0x0047a130L);
        header("MM â€” _MISSIONInit2 (0x480b50)");
        dumpAt(0x00480b50L);
        header("MM â€” MC loader (0x481940)");
        dumpAt(0x00481940L);
        header("MM â€” ParseLayerFile (0x4b4370)");
        dumpAt(0x004b4370L);
        header("MM â€” GetLayerByIndex (0x4b3170)");
        dumpAt(0x004b3170L);
        header("MM â€” callers of FUN_0047a130");
        dumpCallers(0x0047a130L);
        header("MM â€” callers of MC loader");
        dumpCallers(0x00481940L);
        header("MM â€” range 0x481800-0x482200");
        dumpRange(0x00481800L, 0x00482200L);
        header("MM â€” string search: MM keywords");
        searchStrings(new String[]{"textFormat", ".T2", ".LAY", "tmap", "tdic", ".MM"});
    }

    // -----------------------------------------------------------------------
    // BI â€” bytecode interpreter / AI scripts
    // -----------------------------------------------------------------------
    private void analyzeBI() throws Exception {
        header("BI â€” _CTExecProgram@4 (0x466970)");
        dumpAt(0x00466970L);
        header("BI â€” opcode dispatcher FUN_00466a80");
        dumpAtForced(0x00466a80L);
        header("BI â€” _CTDo_move (0x465cc0)");
        dumpAtForced(0x00465cc0L);
        header("BI â€” _CTDo_movetoalt (0x465e20)");
        dumpAtForced(0x00465e20L);
        header("BI â€” _CTDo_jink (0x4663f0)");
        dumpAtForced(0x004663f0L);
        header("BI â€” _CTDo_maneuver (0x465a70)");
        dumpAtForced(0x00465a70L);
        header("BI â€” _CTDo_turn (0x465ea0)");
        dumpAtForced(0x00465ea0L);
        header("BI â€” _CTEval_ir (0x4650e0)");
        dumpAt(0x004650e0L);
        header("BI â€” _CTEval_radar (0x4650a0)");
        dumpAt(0x004650a0L);
        header("BI â€” _MVRJink@40 (0x4ac9e0)");
        dumpAt(0x004ac9e0L);
        header("BI â€” _MVRMove (0x4ac510)");
        dumpAt(0x004ac510L);
        header("BI â€” _CreateMove (0x463a20)");
        dumpAt(0x00463a20L);
        header("BI â€” callers of _CTExecProgram@4");
        dumpCallers(0x00466970L);
        header("BI â€” range near interpreter 0x466800-0x466a00");
        dumpRange(0x00466800L, 0x00466a00L);
        header("BI â€” arg reader range 0x465c00-0x465f00");
        dumpRange(0x00465c00L, 0x00465f00L);
    }

    // -----------------------------------------------------------------------
    // ECM â€” electronic countermeasures
    // -----------------------------------------------------------------------
    private void analyzeECM() throws Exception {
        header("ECM â€” FUN_00452770 (evaluator)");
        dumpAt(0x00452770L);
        header("ECM â€” FUN_004d5e58 (geometry)");
        dumpAt(0x004d5e58L);
        header("ECM â€” @HARDFindJammer (0x452ea0)");
        dumpAt(0x00452ea0L);
        header("ECM â€” FUN_00452980");
        dumpAt(0x00452980L);
        header("ECM â€” callers of evaluator");
        dumpCallers(0x00452770L);
        header("ECM â€” callers of geometry");
        dumpCallers(0x004d5e58L);
        header("ECM â€” bit 0x20 scan 0x452000-0x454000");
        for (long va : findFunctionsWithMask(0x00452000L, 0x00454000L, 0x20L)) dumpAt(va);
        header("ECM â€” bit 0x40 scan");
        for (long va : findFunctionsWithMask(0x00452000L, 0x00454000L, 0x40L)) dumpAt(va);
        header("ECM â€” bit 0x80 scan");
        for (long va : findFunctionsWithMask(0x00452000L, 0x00454000L, 0x80L)) dumpAt(va);
    }

    // -----------------------------------------------------------------------
    // HGR â€” hangar / airbase rendering
    // -----------------------------------------------------------------------
    private void analyzeHGR() throws Exception {
        header("HGR â€” FUN_004543c0 (loader)");
        dumpAt(0x004543c0L);
        header("HGR â€” FUN_004809d0");
        dumpAt(0x004809d0L);
        header("HGR â€” FUN_00480150 / 004801a0");
        dumpAt(0x00480150L); dumpAt(0x004801a0L);
        header("HGR â€” _G_TileInit (0x447a40)");
        dumpAt(0x00447a40L);
        header("HGR â€” @G_Tile@32 (0x447aa5)");
        dumpAt(0x00447aa5L);
        header("HGR â€” _T_Init2 (0x4c5f60)");
        dumpAt(0x004c5f60L);
        header("HGR â€” callers of loader");
        dumpCallers(0x004543c0L);
        header("HGR â€” range 0x480000-0x480600");
        dumpRange(0x00480000L, 0x00480600L);
    }

    // -----------------------------------------------------------------------
    // MUS â€” music / sound sequencer
    // -----------------------------------------------------------------------
    private void analyzeMUS() throws Exception {
        header("MUS â€” _SEQmusic (0x446b70)");
        dumpAtForced(0x00446b70L);
        header("MUS â€” _SEQfadein (0x446890)");
        dumpAtForced(0x00446890L);
        header("MUS â€” _SEQfadeout (0x446910)");
        dumpAtForced(0x00446910L);
        header("MUS â€” ?SeqFadeOut (0x446990)");
        dumpAtForced(0x00446990L);
        header("MUS â€” ?SeqFadeIn (0x4469f0)");
        dumpAtForced(0x004469f0L);
        header("MUS â€” ?MusicOn (0x4329e0)");
        dumpAt(0x004329e0L);
        header("MUS â€” ?MusicOff (0x432c00)");
        dumpAt(0x00432c00L);
        header("MUS â€” ?ShellMusicUpdate (0x432f80)");
        dumpAt(0x00432f80L);
        header("MUS â€” ?InitMusic (0x4328b0)");
        dumpAtForced(0x004328b0L);
        header("MUS â€” loaders 0x4a6ae0/4a6b50/4a7180");
        dumpAt(0x004a6ae0L); dumpAt(0x004a6b50L); dumpAt(0x004a7180L);
    }

    // -----------------------------------------------------------------------
    // OTNT â€” OT/NT vehicle classification and AI
    // -----------------------------------------------------------------------
    private void analyzeOTNT() throws Exception {
        header("OTNT â€” _GVProc (0x473db0)");
        dumpAt(0x00473db0L);
        header("OTNT â€” FUN_004bed70");
        dumpAt(0x004bed70L);
        header("OTNT â€” FUN_004747c0");
        dumpAt(0x004747c0L);
        header("OTNT â€” priority masks 0x8000/0x40000/0x80000/0x100000/0x400000");
        for (long m : new long[]{0x8000L, 0x40000L, 0x80000L, 0x100000L, 0x400000L})
            for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, m)) dumpAt(va);
        header("OTNT â€” capability masks 0x20/0x100/0x200/0x400/0x800");
        for (long m : new long[]{0x20L, 0x100L, 0x200L, 0x400L, 0x800L})
            for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, m)) dumpAt(va);
    }

    // -----------------------------------------------------------------------
    // T2 â€” terrain tile system
    // -----------------------------------------------------------------------
    private void analyzeT2() throws Exception {
        header("T2 â€” @G_Tile@32 (0x447aa5)");
        dumpAt(0x00447aa5L);
        header("T2 â€” _G_TileInit (0x447a40)");
        dumpAt(0x00447a40L);
        header("T2 â€” do_use_terrain_detail (0x4d2344)");
        dumpAt(0x004d2344L);
        header("T2 â€” ?MAPWorldToScreen (0x422380)");
        dumpAt(0x00422380L);
        header("T2 â€” _GetGround@0 (0x47af70)");
        dumpAt(0x0047af70L);
        header("T2 â€” callers of @G_Tile@32");
        dumpCallers(0x00447aa5L);
        header("T2 â€” tile cluster 0x447a00-0x447f00");
        dumpRange(0x00447a00L, 0x00447f00L);
        header("T2 â€” MM/lib area 0x479e00-0x47a600");
        dumpRange(0x00479e00L, 0x0047a600L);
        header("T2 â€” string search: BIT2 / .T2 / tmap / tdic");
        searchStrings(new String[]{".T2", "tmap", "tdic"});
        header("T2 â€” T2 sub-header constant scan");
        for (long c : new long[]{0x95L, 0x80L, 195L, 21L})
            for (long va : findFunctionsReadingOffsets(0x00400000L, 0x00600000L, (int)c, (int)c))
                dumpAt(va);
    }

    // -----------------------------------------------------------------------
    // GAS â€” fuel, hardpoints, BRF, vehicle weapons
    // -----------------------------------------------------------------------
    private void analyzeGAS() throws Exception {
        header("GAS â€” _GVProc (0x473db0)");
        dumpAt(0x00473db0L);
        header("GAS â€” _HARDPtrs@12 (0x452770)");
        dumpAt(0x00452770L);
        header("GAS â€” _HARDFindProj@16 (0x452ff0)");
        dumpAt(0x00452ff0L);
        header("GAS â€” @FMFuelConsumption (0x451e50)");
        dumpAt(0x00451e50L);
        header("GAS â€” _BurnFuel (0x451e80)");
        dumpAt(0x00451e80L);
        header("GAS â€” @FMBurnNPCFuel (0x452050)");
        dumpAt(0x00452050L);
        header("GAS â€” _HARDTotalFuel (0x453a70)");
        dumpAt(0x00453a70L);
        header("GAS â€” ?MPSetFuel (0x4723a0)");
        dumpAt(0x004723a0L);
        header("GAS â€” @HARDFindECMForObj (0x452f10)");
        dumpAt(0x00452f10L);
        header("GAS â€” @HARDBestSeeker (0x452e60)");
        dumpAt(0x00452e60L);
        header("GAS â€” @HARDBestSeekers (0x452d90)");
        dumpAt(0x00452d90L);
        header("GAS â€” _Seek (0x4ad090)");
        dumpAt(0x004ad090L);
        header("GAS â€” _DAMAGEDoHit (0x40f970)");
        dumpAt(0x0040f970L);
        header("GAS â€” _SetupJT (0x4a7230)");
        dumpAt(0x004a7230L);
        header("GAS â€” callers of _BurnFuel");
        dumpCallers(0x00451e80L);
        header("GAS â€” callers of _HARDFindProj@16");
        dumpCallers(0x00452ff0L);
        header("GAS â€” FUN_00473f50 / 00473be0");
        dumpAt(0x00473f50L); dumpAt(0x00473be0L);
    }
}
