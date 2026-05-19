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
        analyzeCAM();
        analyzeMC();
        analyzeT2DLL();

        // New subsystem coverage -- dark zones
        analyzeGameLoop();
        analyzeRenderer();
        analyzePhysics();
        analyzeNetwork();
        analyzeInput();

        // BI/AI -- FRAME opcode consumer search
        analyzeBIFRAME();

        closeOutput();
    }

    // -----------------------------------------------------------------------
    // LAY  --  sky / atmosphere layer system
    // -----------------------------------------------------------------------
    private void analyzeLAY() throws Exception {
        header("LAY  --  ParseLayerFile (0x4b4370)");
        dumpAt(0x004b4370L);
        header("LAY  --  CopyLayersToRuntime (0x4b3750)");
        dumpAt(0x004b3750L);
        header("LAY  --  InterpolateLayers (0x4b3820)");
        dumpAt(0x004b3820L);
        header("LAY  --  GetLayerAtAltitude (0x4b3be0)");
        dumpAt(0x004b3be0L);
        header("LAY  --  T_DefaultHorizon (0x4aacf0)");
        dumpAt(0x004aacf0L);
        header("LAY  --  ApplyBrightnessGradient (0x4b3cb0)");
        dumpAt(0x004b3cb0L);
        header("LAY  --  UpdateSkyState (0x4b3d90)");
        dumpAt(0x004b3d90L);
        header("LAY  --  UpdateAuroraClouds (0x4b4170)");
        dumpAt(0x004b4170L);
        header("LAY  --  FindNearestColorEntry (0x4b3ad0)");
        dumpAt(0x004b3ad0L);
        header("LAY  --  LoadPICByWildcard (0x4b4680)");
        dumpAt(0x004b4680L);
        header("LAY  --  GetLayerBoundary (0x4b3190)");
        dumpAt(0x004b3190L);
        header("LAY  --  GetLayerByIndex (0x4b3170)");
        dumpAt(0x004b3170L);
        header("LAY  --  WRFogLayerUpdate (0x4b4320)");
        dumpAt(0x004b4320L);
        header("LAY  --  all callers of ParseLayerFile");
        dumpCallers(0x004b4370L);
        header("LAY  --  all callers of WRFogLayerUpdate");
        dumpCallers(0x004b4320L);
        header("LAY  --  range 0x4b2ea0-0x4b4200");
        dumpRange(0x004b2ea0L, 0x004b4200L);
    }

    // -----------------------------------------------------------------------
    // HUD  --  heads-up display
    // -----------------------------------------------------------------------
    private void analyzeHUD() throws Exception {
        header("HUD  --  draw dispatcher / init (0x406040)");
        dumpAt(0x00406040L);
        header("HUD  --  warning lights FUN_00407930");
        dumpAt(0x00407930L);
        header("HUD  --  FUN_00407ee0");
        dumpAt(0x00407ee0L);
        header("HUD  --  FUN_00408420");
        dumpAt(0x00408420L);
        header("HUD  --  FUN_00407a00");
        dumpAt(0x00407a00L);
        header("HUD  --  FUN_00416380");
        dumpAt(0x00416380L);
        header("HUD  --  _PLANECheckFuel@0 (0x49fb70)");
        dumpAt(0x0049fb70L);
        header("HUD  --  _PROJProc (0x4c1f50)");
        dumpAtForced(0x004c1f50L);
        header("HUD  --  PROJMoveProc (0x4c11b0)");
        dumpAtForced(0x004c11b0L);
        header("HUD  --  callers of _HARDPtrs@12 (0x452770)");
        dumpCallers(0x00452770L);
        header("HUD  --  draw range 0x407b60-0x40ac00");
        dumpRange(0x00407b60L, 0x0040ac00L);
        header("HUD  --  0x4000 (bit14) writers 0x400000-0x500000");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00500000L, 0x4000L)) dumpAt(va);
    }

    // -----------------------------------------------------------------------
    // DLG  --  dialog / UI system
    // -----------------------------------------------------------------------
    private void analyzeDLG() throws Exception {
        header("DLG  --  _DialogWhatItem (0x488fc0)");
        dumpAtForced(0x00488fc0L);
        header("DLG  --  _DialogShow (0x4880d0)");
        dumpAt(0x004880d0L);
        header("DLG  --  _DialogShutDown (0x488190)");
        dumpAt(0x00488190L);
        header("DLG  --  _DialogDone (0x488300)");
        dumpAt(0x00488300L);
        header("DLG  --  @DialogGetPtr (0x4892e0)");
        dumpAt(0x004892e0L);
        header("DLG  --  @DialogGetValue (0x489300)");
        dumpAt(0x00489300L);
        header("DLG  --  @DialogSelectItem (0x4894f0)");
        dumpAt(0x004894f0L);
        header("DLG  --  @DialogDeselectItem (0x489580)");
        dumpAtForced(0x00489580L);
        header("DLG  --  _ChoosePreload (0x4897f0)");
        dumpAt(0x004897f0L);
        header("DLG  --  _ChooseActivity (0x4a08a0)");
        dumpAt(0x004a08a0L);
        header("DLG  --  FUN_004a6e20 (dispatcher)");
        dumpAt(0x004a6e20L);
        header("DLG  --  draw: _DrawText/Action/FormattedText/CampaignList/Rocker/EditBox");
        dumpAt(0x00489ac0L); dumpAt(0x00489b90L); dumpAt(0x0048a910L);
        dumpAt(0x0048abf0L); dumpAt(0x0048b4e0L); dumpAt(0x0048c710L);
        header("DLG  --  callers of _ChoosePreload");
        dumpCallers(0x004897f0L);
        header("DLG  --  callers of dispatcher");
        dumpCallers(0x004a6e20L);
    }

    // -----------------------------------------------------------------------
    // PROJ  --  projectile / missile system
    // -----------------------------------------------------------------------
    private void analyzePROJ() throws Exception {
        header("PROJ  --  _PROJInit@0 (0x4c06a0)");
        dumpAt(0x004c06a0L);
        header("PROJ  --  _PROJAdd@40 (0x4c0a90)");
        dumpAt(0x004c0a90L);
        header("PROJ  --  _PROJFire@16 (0x4c2170)");
        dumpAt(0x004c2170L);
        header("PROJ  --  _PROJProc (0x4c1f50)");
        dumpAtForced(0x004c1f50L);
        header("PROJ  --  ?PROJMoveProc (0x4c11b0)");
        dumpAtForced(0x004c11b0L);
        header("PROJ  --  _PROJSpeed@8 (0x4c1120)");
        dumpAt(0x004c1120L);
        header("PROJ  --  _PROJEngineState@0 (0x4c1170)");
        dumpAt(0x004c1170L);
        header("PROJ  --  _PROJLockUpdate@0 (0x4c0960)");
        dumpAt(0x004c0960L);
        header("PROJ  --  _PROJHitChance@28 (0x4c3380)");
        dumpAt(0x004c3380L);
        header("PROJ  --  _PROJLock@24 (0x4c2f20)");
        dumpAt(0x004c2f20L);
        header("PROJ  --  _DAMAGEDoHit (0x40f970)");
        dumpAt(0x0040f970L);
        header("PROJ  --  proximity fuze (0x4c3960)");
        dumpAt(0x004c3960L);
        header("PROJ  --  offset scan 0x50-0x7f");
        for (long va : findFunctionsReadingOffsets(0x00400000L, 0x00500000L, 0x50, 0x7F)) dumpAt(va);
        header("PROJ  --  cluster 0x4c0000-0x4c3000");
        dumpRange(0x004c0000L, 0x004c3000L);
    }

    // -----------------------------------------------------------------------
    // SEE  --  seeker / missile guidance
    // -----------------------------------------------------------------------
    private void analyzeSEE() throws Exception {
        header("SEE  --  seeker cone / _PROJInFOV (0x4c2860)");
        dumpAt(0x004c2860L);
        header("SEE  --  search lobe (0x4c2eb0)");
        dumpAt(0x004c2eb0L);
        header("SEE  --  track lobe (0x4c31f0)");
        dumpAt(0x004c31f0L);
        header("SEE  --  pre-lobe (0x4c24b0)");
        dumpAt(0x004c24b0L);
        header("SEE  --  guidance final (0x4c26f0)");
        dumpAt(0x004c26f0L);
        header("SEE  --  lock-acquire 1 (0x4c5000)");
        dumpAt(0x004c5000L);
        header("SEE  --  lock-acquire 2 (0x4c5050)");
        dumpAt(0x004c5050L);
        header("SEE  --  target selection (0x4c52d0)");
        dumpAt(0x004c52d0L);
        header("SEE  --  missile flag handler (0x4c3eb0)");
        dumpAt(0x004c3eb0L);
        header("SEE  --  re-eval (0x4c4100)");
        dumpAt(0x004c4100L);
        header("SEE  --  wrapper (0x4c58a0)");
        dumpAt(0x004c58a0L);
        header("SEE  --  callers of seeker cone");
        dumpCallers(0x004c2860L);
        header("SEE  --  0x20000 flag scan 0x4c0000-0x4c7000");
        for (long va : findFunctionsWithMask(0x004c0000L, 0x004c7000L, 0x20000L)) dumpAt(va);
        header("SEE  --  0x100000 flag scan");
        for (long va : findFunctionsWithMask(0x004c0000L, 0x004c7000L, 0x100000L)) dumpAt(va);
    }

    // -----------------------------------------------------------------------
    // MM  --  mission map / campaign
    // -----------------------------------------------------------------------
    private void analyzeMM() throws Exception {
        header("MM  --  keyword handler FUN_0047a510");
        dumpAt(0x0047a510L);
        header("MM  --  FUN_0047a130 (line parser)");
        dumpAt(0x0047a130L);
        header("MM  --  _MISSIONInit2 (0x480b50)");
        dumpAt(0x00480b50L);
        header("MM  --  MC loader (0x481940)");
        dumpAt(0x00481940L);
        header("MM  --  ParseLayerFile (0x4b4370)");
        dumpAt(0x004b4370L);
        header("MM  --  GetLayerByIndex (0x4b3170)");
        dumpAt(0x004b3170L);
        header("MM  --  callers of FUN_0047a130");
        dumpCallers(0x0047a130L);
        header("MM  --  callers of MC loader");
        dumpCallers(0x00481940L);
        header("MM  --  range 0x481800-0x482200");
        dumpRange(0x00481800L, 0x00482200L);
        header("MM  --  string search: MM keywords");
        searchStrings(new String[]{"textFormat", ".T2", ".LAY", "tmap", "tdic", ".MM"});
    }

    // -----------------------------------------------------------------------
    // BI  --  bytecode interpreter / AI scripts
    // -----------------------------------------------------------------------
    private void analyzeBI() throws Exception {
        header("BI  --  _CTExecProgram@4 (0x466970)");
        dumpAt(0x00466970L);
        header("BI  --  opcode dispatcher FUN_00466a80");
        dumpAtForced(0x00466a80L);
        header("BI  --  _CTDo_move (0x465cc0)");
        dumpAtForced(0x00465cc0L);
        header("BI  --  _CTDo_movetoalt (0x465e20)");
        dumpAtForced(0x00465e20L);
        header("BI  --  _CTDo_jink (0x4663f0)");
        dumpAtForced(0x004663f0L);
        header("BI  --  _CTDo_maneuver (0x465a70)");
        dumpAtForced(0x00465a70L);
        header("BI  --  _CTDo_turn (0x465ea0)");
        dumpAtForced(0x00465ea0L);
        header("BI  --  _CTEval_ir (0x4650e0)");
        dumpAt(0x004650e0L);
        header("BI  --  _CTEval_radar (0x4650a0)");
        dumpAt(0x004650a0L);
        header("BI  --  _MVRJink@40 (0x4ac9e0)");
        dumpAt(0x004ac9e0L);
        header("BI  --  _MVRMove (0x4ac510)");
        dumpAt(0x004ac510L);
        header("BI  --  _CreateMove (0x463a20)");
        dumpAt(0x00463a20L);
        header("BI  --  callers of _CTExecProgram@4");
        dumpCallers(0x00466970L);
        header("BI  --  range near interpreter 0x466800-0x466a00");
        dumpRange(0x00466800L, 0x00466a00L);
        header("BI  --  arg reader range 0x465c00-0x465f00");
        dumpRange(0x00465c00L, 0x00465f00L);
    }

    // -----------------------------------------------------------------------
    // ECM  --  electronic countermeasures
    // -----------------------------------------------------------------------
    private void analyzeECM() throws Exception {
        header("ECM  --  FUN_00452770 (evaluator)");
        dumpAt(0x00452770L);
        header("ECM  --  FUN_004d5e58 (geometry)");
        dumpAt(0x004d5e58L);
        header("ECM  --  @HARDFindJammer (0x452ea0)");
        dumpAt(0x00452ea0L);
        header("ECM  --  FUN_00452980");
        dumpAt(0x00452980L);
        header("ECM  --  callers of evaluator");
        dumpCallers(0x00452770L);
        header("ECM  --  callers of geometry");
        dumpCallers(0x004d5e58L);
        header("ECM  --  bit 0x20 scan 0x452000-0x454000");
        for (long va : findFunctionsWithMask(0x00452000L, 0x00454000L, 0x20L)) dumpAt(va);
        header("ECM  --  bit 0x40 scan");
        for (long va : findFunctionsWithMask(0x00452000L, 0x00454000L, 0x40L)) dumpAt(va);
        header("ECM  --  bit 0x80 scan");
        for (long va : findFunctionsWithMask(0x00452000L, 0x00454000L, 0x80L)) dumpAt(va);
    }

    // -----------------------------------------------------------------------
    // HGR  --  hangar / airbase rendering
    // -----------------------------------------------------------------------
    private void analyzeHGR() throws Exception {
        header("HGR  --  FUN_004543c0 (loader)");
        dumpAt(0x004543c0L);
        header("HGR  --  FUN_004809d0");
        dumpAt(0x004809d0L);
        header("HGR  --  FUN_00480150 / 004801a0");
        dumpAt(0x00480150L); dumpAt(0x004801a0L);
        header("HGR  --  _G_TileInit (0x447a40)");
        dumpAt(0x00447a40L);
        header("HGR  --  @G_Tile@32 (0x447aa5)");
        dumpAt(0x00447aa5L);
        header("HGR  --  _T_Init2 (0x4c5f60)");
        dumpAt(0x004c5f60L);
        header("HGR  --  callers of loader");
        dumpCallers(0x004543c0L);
        header("HGR  --  range 0x480000-0x480600");
        dumpRange(0x00480000L, 0x00480600L);
    }

    // -----------------------------------------------------------------------
    // MUS  --  music / sound sequencer
    // -----------------------------------------------------------------------
    private void analyzeMUS() throws Exception {
        header("MUS  --  _SEQmusic (0x446b70)");
        dumpAtForced(0x00446b70L);
        header("MUS  --  _SEQfadein (0x446890)");
        dumpAtForced(0x00446890L);
        header("MUS  --  _SEQfadeout (0x446910)");
        dumpAtForced(0x00446910L);
        header("MUS  --  ?SeqFadeOut (0x446990)");
        dumpAtForced(0x00446990L);
        header("MUS  --  ?SeqFadeIn (0x4469f0)");
        dumpAtForced(0x004469f0L);
        header("MUS  --  ?MusicOn (0x4329e0)");
        dumpAt(0x004329e0L);
        header("MUS  --  ?MusicOff (0x432c00)");
        dumpAt(0x00432c00L);
        header("MUS  --  ?ShellMusicUpdate (0x432f80)");
        dumpAt(0x00432f80L);
        header("MUS  --  ?InitMusic (0x4328b0)");
        dumpAtForced(0x004328b0L);
        header("MUS  --  loaders 0x4a6ae0/4a6b50/4a7180");
        dumpAt(0x004a6ae0L); dumpAt(0x004a6b50L); dumpAt(0x004a7180L);
    }

    // -----------------------------------------------------------------------
    // OTNT  --  OT/NT vehicle classification and AI
    // -----------------------------------------------------------------------
    private void analyzeOTNT() throws Exception {
        header("OTNT  --  _GVProc (0x473db0)");
        dumpAt(0x00473db0L);
        header("OTNT  --  FUN_004bed70");
        dumpAt(0x004bed70L);
        header("OTNT  --  FUN_004747c0");
        dumpAt(0x004747c0L);
        header("OTNT  --  priority masks 0x8000/0x40000/0x80000/0x100000/0x400000");
        for (long m : new long[]{0x8000L, 0x40000L, 0x80000L, 0x100000L, 0x400000L})
            for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, m)) dumpAt(va);
        header("OTNT  --  capability masks 0x20/0x100/0x200/0x400/0x800");
        for (long m : new long[]{0x20L, 0x100L, 0x200L, 0x400L, 0x800L})
            for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, m)) dumpAt(va);
    }

    // -----------------------------------------------------------------------
    // T2  --  terrain tile system
    // -----------------------------------------------------------------------
    private void analyzeT2() throws Exception {
        header("T2  --  @G_Tile@32 (0x447aa5)");
        dumpAt(0x00447aa5L);
        header("T2  --  _G_TileInit (0x447a40)");
        dumpAt(0x00447a40L);
        header("T2  --  do_use_terrain_detail (0x4d2344)");
        dumpAt(0x004d2344L);
        header("T2  --  ?MAPWorldToScreen (0x422380)");
        dumpAt(0x00422380L);
        header("T2  --  _GetGround@0 (0x47af70)");
        dumpAt(0x0047af70L);
        header("T2  --  callers of @G_Tile@32");
        dumpCallers(0x00447aa5L);
        header("T2  --  tile cluster 0x447a00-0x447f00");
        dumpRange(0x00447a00L, 0x00447f00L);
        header("T2  --  MM/lib area 0x479e00-0x47a600");
        dumpRange(0x00479e00L, 0x0047a600L);
        header("T2  --  string search: BIT2 / .T2 / tmap / tdic");
        searchStrings(new String[]{".T2", "tmap", "tdic"});
        header("T2  --  T2 sub-header constant scan");
        for (long c : new long[]{0x95L, 0x80L, 195L, 21L})
            for (long va : findFunctionsReadingOffsets(0x00400000L, 0x00600000L, (int)c, (int)c))
                dumpAt(va);
    }

    // -----------------------------------------------------------------------
    // CAM  --  campaign DLL binary layout
    // -----------------------------------------------------------------------
    private void analyzeCAM() throws Exception {
        header("CAM  --  campaign launcher FUN_00428412");
        dumpAtForced(0x00428412L);
        header("CAM  --  callers of campaign launcher");
        dumpCallers(0x00428412L);
        header("CAM  --  MC loader (0x481940)");
        dumpAt(0x00481940L);
        header("CAM  --  _ChooseActivity (0x4a08a0)");
        dumpAt(0x004a08a0L);
        header("CAM  --  campaign range 0x428000-0x430000");
        dumpRange(0x00428000L, 0x00430000L);
        header("CAM  --  asset loader area 0x4a6cc0-0x4a7200");
        dumpRange(0x004a6cc0L, 0x004a7200L);
        header("CAM  --  string search: campaign / theater keywords");
        searchStrings(new String[]{".CAM", "BALTIC", "PERSIAN", "KOREA",
                "CHINA", "EGYPT", "VIETNAM", "campaign", "theater"});
    }

    // -----------------------------------------------------------------------
    // MC  --  mission condition DLL flow
    // -----------------------------------------------------------------------
    private void analyzeMC() throws Exception {
        header("MC  --  MC loader (0x481940)");
        dumpAt(0x00481940L);
        header("MC  --  callers of MC loader");
        dumpCallers(0x00481940L);
        header("MC  --  _MISSIONInit2 (0x480b50)");
        dumpAt(0x00480b50L);
        header("MC  --  MM keyword handler FUN_0047a510");
        dumpAt(0x0047a510L);
        header("MC  --  mission range 0x480000-0x486000");
        dumpRange(0x00480000L, 0x00486000L);
        header("MC  --  trigger/event range 0x486000-0x489000");
        dumpRange(0x00486000L, 0x00489000L);
        header("MC  --  string search: condition keywords");
        searchStrings(new String[]{".MC", "COND", "TRIGGER", "DESTROY",
                "CAPTURE", "SUCCEED", "FAIL"});
    }

    // -----------------------------------------------------------------------
    // T2DLL  --  T2 terrain overlay DLL / surface-class mapping
    // -----------------------------------------------------------------------
    private void analyzeT2DLL() throws Exception {
        header("T2DLL  --  do_use_terrain_detail (0x4d2344)");
        dumpAt(0x004d2344L);
        header("T2DLL  --  callers of do_use_terrain_detail");
        dumpCallers(0x004d2344L);
        header("T2DLL  --  tileExpand (0x4f4f78)");
        dumpAt(0x004f4f78L);
        header("T2DLL  --  expandTerrain (0x50e145)");
        dumpAt(0x0050e145L);
        header("T2DLL  --  T2 DLL load area 0x4d2000-0x4d5000");
        dumpRange(0x004d2000L, 0x004d5000L);
        header("T2DLL  --  tileExpand area 0x4f4000-0x4f6000");
        dumpRange(0x004f4000L, 0x004f6000L);
        header("T2DLL  --  sub-header constant 0x95 scan 0x4d0000-0x510000");
        for (long va : findFunctionsWithMask(0x004d0000L, 0x00510000L, 0x95L)) dumpAt(va);
        header("T2DLL  --  surface-class offset scan (0-0x20) in T2 area");
        for (long va : findFunctionsReadingOffsets(0x004d0000L, 0x00510000L, 0, 0x20)) dumpAt(va);
        header("T2DLL  --  string search: BIT2 / T2 terrain keywords");
        searchStrings(new String[]{"BIT2", ".T2", "tmap", "tdic", "terrain"});
    }

    // -----------------------------------------------------------------------
    // GAS  --  fuel, hardpoints, BRF, vehicle weapons
    // -----------------------------------------------------------------------
    private void analyzeGAS() throws Exception {
        header("GAS  --  _GVProc (0x473db0)");
        dumpAt(0x00473db0L);
        header("GAS  --  _HARDPtrs@12 (0x452770)");
        dumpAt(0x00452770L);
        header("GAS  --  _HARDFindProj@16 (0x452ff0)");
        dumpAt(0x00452ff0L);
        header("GAS  --  @FMFuelConsumption (0x451e50)");
        dumpAt(0x00451e50L);
        header("GAS  --  _BurnFuel (0x451e80)");
        dumpAt(0x00451e80L);
        header("GAS  --  @FMBurnNPCFuel (0x452050)");
        dumpAt(0x00452050L);
        header("GAS  --  _HARDTotalFuel (0x453a70)");
        dumpAt(0x00453a70L);
        header("GAS  --  ?MPSetFuel (0x4723a0)");
        dumpAt(0x004723a0L);
        header("GAS  --  @HARDFindECMForObj (0x452f10)");
        dumpAt(0x00452f10L);
        header("GAS  --  @HARDBestSeeker (0x452e60)");
        dumpAt(0x00452e60L);
        header("GAS  --  @HARDBestSeekers (0x452d90)");
        dumpAt(0x00452d90L);
        header("GAS  --  _Seek (0x4ad090)");
        dumpAt(0x004ad090L);
        header("GAS  --  _DAMAGEDoHit (0x40f970)");
        dumpAt(0x0040f970L);
        header("GAS  --  _SetupJT (0x4a7230)");
        dumpAt(0x004a7230L);
        header("GAS  --  callers of _BurnFuel");
        dumpCallers(0x00451e80L);
        header("GAS  --  callers of _HARDFindProj@16");
        dumpCallers(0x00452ff0L);
        header("GAS  --  FUN_00473f50 / 00473be0");
        dumpAt(0x00473f50L); dumpAt(0x00473be0L);
    }

    // -----------------------------------------------------------------------
    // GAMELOOP  --  main game loop, per-frame object dispatcher, init
    // -----------------------------------------------------------------------
    private void analyzeGameLoop() throws Exception {
        header("INIT  --  WinMain / _main entry point");
        dumpSymbolsMatching("winmain", "_main", "maincrtstartup", "wwinmain");
        searchStrings(new String[]{"WinMain", "GameLoop", "JANES", "Fighters Anthology"});
        header("INIT  --  game initialization sequence");
        dumpSymbolsMatching("initgame", "_initgame", "gameinit", "_gameinit",
                "startgame", "_startgame", "gamestart", "_gamestart");
        header("LOOP  --  main game loop body");
        dumpSymbolsMatching("gameloop", "_gameloop", "mainloop", "_mainloop",
                "frameupdate", "_frameupdate", "gametick", "_gametick");
        header("LOOP  --  frame counter and timing");
        dumpSymbolsMatching("framecounter", "frametimer", "gametimer", "_gametimer",
                "timestep", "_timestep", "tickcount", "_tickcount");
        header("LOOP  --  per-frame dispatcher range 0x468000-0x479DFF");
        dumpRange(0x00468000L, 0x00479DFFL);
        header("OBJ  --  object add / remove / iterate");
        dumpSymbolsMatching("_objadd", "objadd", "_objremove", "objremove",
                "_objinit", "objinit", "_objproc", "_objupdate", "objupdate",
                "_objlist", "objlist", "_entityadd", "entityadd");
        header("OBJ  --  callers of _GVProc (0x473db0)");
        dumpCallers(0x00473db0L);
        header("OBJ  --  callers of _PROJProc (0x4c1f50)");
        dumpCallers(0x004c1f50L);
        header("INIT  --  range 0x401000-0x406000 (entry point cluster)");
        dumpRange(0x00401000L, 0x00406000L);
        header("INIT  --  campaign and mission init");
        dumpSymbolsMatching("_missioninit", "missioninit", "_campaigninit", "campaigninit",
                "_missionstart", "missionstart", "_levelload", "levelload");
        header("SHUTDOWN  --  game shutdown and cleanup");
        dumpSymbolsMatching("shutdown", "_shutdown", "gameshutdown", "_gameshutdown",
                "cleanup", "_cleanup", "exitgame", "_exitgame");
    }

    // -----------------------------------------------------------------------
    // RENDERER  --  3D rendering pipeline, shape/sprite system, camera
    // -----------------------------------------------------------------------
    private void analyzeRenderer() throws Exception {
        header("RENDER  --  scene dispatch and render loop");
        dumpSymbolsMatching("renderscene", "_renderscene", "drawscene", "_drawscene",
                "rendworld", "_rendworld", "renderframe", "_renderframe",
                "drawworld", "_drawworld", "renderall", "_renderall");
        header("RENDER  --  callers of T_DefaultHorizon (0x4aacf0)");
        dumpCallers(0x004aacf0L);
        header("RENDER  --  callers of @G_Tile@32 (0x447aa5)");
        dumpCallers(0x00447aa5L);
        header("SHAPE  --  shape load and cache");
        dumpSymbolsMatching("_shload", "shload", "_loadsh", "loadsh",
                "_shapeinit", "shapeinit", "_shrender", "shrender");
        searchStrings(new String[]{".SH", ".sh", "wave1", "SH\0"});
        header("SHAPE  --  range 0x4B4200-0x4BEDFF");
        dumpRange(0x004b4200L, 0x004bedffL);
        header("POLY  --  polygon submission and rasterizer");
        dumpSymbolsMatching("_polygon", "polygon", "_drawpoly", "drawpoly",
                "_vertex", "vertex", "_zbuffer", "zbuffer", "_zbuf", "zbuf");
        header("SPRITE  --  sprite and billboard rendering");
        dumpSymbolsMatching("_sprite", "sprite", "_billboard", "billboard",
                "_drawsprite", "drawsprite", "_particle", "particle");
        header("CAMERA  --  viewport and projection");
        dumpSymbolsMatching("_camera", "camera", "_viewport", "viewport",
                "_projection", "projection", "_frustum", "frustum",
                "_setcamera", "setcamera", "_lookat", "lookat");
        searchStrings(new String[]{"camera", "Camera", "viewport", "Viewport"});
        header("CULL  --  visibility culling");
        dumpSymbolsMatching("_cull", "cull", "_visible", "visible",
                "_frustumcull", "frustumcull", "_lod", "lod", "_inview", "inview");
        header("DDRAW  --  DirectDraw surface management");
        dumpSymbolsMatching("_ddraw", "ddraw", "_surface", "surface",
                "_flip", "flip", "_blit", "blit", "_pageflip", "pageflip");
        searchStrings(new String[]{"DirectDraw", "IDirectDraw", "CreateSurface"});
        header("WR  --  WR raster rendering subsystem");
        dumpSymbolsMatching("wrsetup", "wrinit", "wrflush", "wrrender",
                "_wrsetremaps", "wrsetremaps", "_wrpoly", "wrpoly");
        header("PIC  --  texture/PIC loading");
        dumpSymbolsMatching("_picload", "picload", "_loadpic", "loadpic",
                "_picrender", "picrender");
        searchStrings(new String[]{".PIC", ".pic"});
    }

    // -----------------------------------------------------------------------
    // PHYSICS  --  flight model, collision detection
    // -----------------------------------------------------------------------
    private void analyzePhysics() throws Exception {
        header("FM  --  flight model subsystem symbols");
        dumpSymbolsMatching("_fmupdate", "fmupdate", "_fminit", "fminit",
                "_lift", "lift", "_drag", "drag", "_thrust", "thrust",
                "_stall", "stall", "_aoa", "aoa", "_airspeed", "airspeed",
                "_mach", "mach", "_altitude", "altitude", "_glimit", "glimit",
                "@fmburnfuel", "fmburnfuel");
        header("FM  --  callers of @FMFuelConsumption (0x451e50)");
        dumpCallers(0x00451e50L);
        header("FM  --  callers of _BurnFuel (0x451e80)");
        dumpCallers(0x00451e80L);
        header("PROJ  --  callers of _PROJProc wrapper (0x4c1f10)");
        dumpCallers(0x004c1f10L);
        header("PROJ  --  _PROJProc dispatch (0x4c1f50)");
        dumpAtForced(0x004c1f50L);
        header("PROJ  --  PROJMoveProc (0x4c11b0)");
        dumpAtForced(0x004c11b0L);
        header("TERRAIN  --  _GetGround@0 (0x47af70) and callers");
        dumpAt(0x0047af70L);
        dumpCallers(0x0047af70L);
        header("TERRAIN  --  do_use_terrain_detail (0x4d2344) and callers");
        dumpAt(0x004d2344L);
        dumpCallers(0x004d2344L);
        header("TERRAIN  --  range 0x4D0000-0x4EFFFF");
        dumpRange(0x004d0000L, 0x004effffL);
        header("COLLIDE  --  collision symbols");
        dumpSymbolsMatching("_collide", "collide", "_collision", "collision",
                "_impact", "impact", "_hitcheck", "hitcheck",
                "_intersect", "intersect", "_overlap", "overlap");
        header("COLLIDE  --  callers of _DAMAGEDoHit (0x40f970)");
        dumpCallers(0x0040f970L);
        header("COLLIDE  --  ?PROJDamageProc (0x4c1870) and callers");
        dumpAt(0x004c1870L);
        dumpCallers(0x004c1870L);
        header("PT  --  PT loader and init");
        dumpSymbolsMatching("_ptload", "ptload", "_loadpt", "loadpt",
                "_ptinit", "ptinit", "_ptread", "ptread");
        searchStrings(new String[]{".PT", ".pt"});
    }

    // -----------------------------------------------------------------------
    // NETWORK  --  multiplayer protocol, CN_INFO struct
    // -----------------------------------------------------------------------
    private void analyzeNetwork() throws Exception {
        header("CN  --  CN_ReadConfig / CN_WriteConfig");
        dumpSymbolsMatching("cn_readconfig", "_cn_readconfig", "cn_writeconfig", "_cn_writeconfig",
                "cnreadconfig", "cnwriteconfig", "cn_init", "_cn_init", "cninit");
        header("CN  --  CN_INFO struct field scan (offsets 0x00-0x200)");
        findFunctionsReadingOffsets(0x00400000L, 0x00540000L, 0x00, 0x200);
        header("MP  --  ?MPReceive (0x46C980) and callers");
        dumpAt(0x0046c980L);
        dumpCallers(0x0046c980L);
        header("MP  --  multiplayer state and symbols");
        dumpSymbolsMatching("mpreceive", "mpsend", "mpupdate", "mpinit",
                "mpframe", "mpsync", "mpstatus", "mpsession",
                "mpjoin", "mphost", "_mprecv", "_mpsend", "_mpupdate");
        header("MP  --  range 0x482200-0x4AACEF");
        dumpRange(0x00482200L, 0x004aacefL);
        header("NET  --  IPX transport");
        dumpSymbolsMatching("_ipxsend", "ipxsend", "_ipxrecv", "ipxrecv",
                "_ipxinit", "ipxinit", "_ipxopen", "ipxopen");
        searchStrings(new String[]{"IPX", "ipx", "Novell"});
        header("NET  --  TCP/IP transport");
        dumpSymbolsMatching("_tcpsend", "tcpsend", "_tcprecv", "tcprecv",
                "_tcpinit", "tcpinit", "_tcpconnect", "tcpconnect");
        searchStrings(new String[]{"TCP", "socket", "winsock", "WinSock"});
        header("NET  --  serial / modem transport");
        dumpSymbolsMatching("_serialsend", "serialsend", "_modemsend", "modemsend",
                "_comminit", "comminit", "_commopen", "commopen");
        searchStrings(new String[]{"COM", "modem", "Modem", "serial"});
        header("NET  --  session management");
        dumpSymbolsMatching("_netsession", "netsession", "_sessioninit", "sessioninit",
                "_sessionjoin", "sessionjoin", "_sessionhost", "sessionhost");
        searchStrings(new String[]{"session", "Session", "lobby", "host", "join"});
        header("MP  --  player/entity sync");
        dumpSymbolsMatching("mpsetfuel", "?mpsetfuel", "mpsetpos", "mpsetstate",
                "mpsetweapon", "mpsetdamage", "mpentityupdate");
    }

    // -----------------------------------------------------------------------
    // BIFRAME  --  BI opcode 0x28 (FRAME) consumer search
    // -----------------------------------------------------------------------
    private void analyzeBIFRAME() throws Exception {
        header("BIFRAME  --  FRAME writer: case 0x28 dispatch area 0x4ace00-0x4ad800");
        dumpRange(0x004ace00L, 0x004ad800L);
        header("BIFRAME  --  CT state save FUN_004668f0");
        dumpAt(0x004668f0L);
        header("BIFRAME  --  CT state restore FUN_00466920");
        dumpAt(0x00466920L);
        header("BIFRAME  --  findFunctionsReadingOffsets +0x7c (FRAME s16 value in CT state block)");
        for (long va : findFunctionsReadingOffsets(0x00400000L, 0x00540000L, 0x7c, 0x80)) dumpAt(va);
        header("BIFRAME  --  _FMFlight@0 (0x47b020) candidate consumer");
        dumpAtForced(0x0047b020L);
        header("BIFRAME  --  _MANAdd@24 (0x47ceb0) candidate consumer");
        dumpAtForced(0x0047ceb0L);
        header("BIFRAME  --  FUN_0048e740 candidate consumer");
        dumpAtForced(0x0048e740L);
        header("BIFRAME  --  _GVDoCurrentWaypoint / MPStatusSet (symbol search)");
        dumpSymbolsMatching("GVDoCurrentWaypoint", "gvdocurrentwaypoint",
                "MPStatusSet", "mpstatusset", "INFO2Draw", "info2draw");
        header("BIFRAME  --  callers of DAT_0050cf90 (CT block saved-copy pointer)");
        dumpCallers(0x0050cf90L);
    }

    // -----------------------------------------------------------------------
    // INPUT  --  joystick, keyboard, mouse, world-coord transforms
    // -----------------------------------------------------------------------
    private void analyzeInput() throws Exception {
        header("INPUT  --  joystick init and poll");
        dumpSymbolsMatching("_joyinit", "joyinit", "_joypoll", "joypoll",
                "_joyread", "joyread", "_joyupdate", "joyupdate",
                "_dinputinit", "dinputinit", "_dinpoll", "dinpoll");
        searchStrings(new String[]{"joystick", "Joystick", "DirectInput",
                "joyGetDevCaps", "joyGetPos"});
        header("INPUT  --  range 0x420000-0x42FFFF");
        dumpRange(0x00420000L, 0x0042ffffL);
        header("INPUT  --  keyboard handling");
        dumpSymbolsMatching("_keyinit", "keyinit", "_keypoll", "keypoll",
                "_keyread", "keyread", "_keydown", "keydown",
                "_keyboard", "keyboard", "_keybind", "keybind");
        searchStrings(new String[]{"GetAsyncKeyState", "GetKeyState"});
        header("INPUT  --  mouse handling");
        dumpSymbolsMatching("_mouseinit", "mouseinit", "_mousepoll", "mousepoll",
                "_mouseread", "mouseread", "_mousepos", "mousepos",
                "_cursorinit", "cursorinit", "_cursor", "cursor");
        searchStrings(new String[]{"GetCursorPos", "SetCursorPos", "ShowCursor"});
        header("INPUT  --  control mapping and deadzone");
        dumpSymbolsMatching("_ctrlmap", "ctrlmap", "_deadzone", "deadzone",
                "_sensitivity", "sensitivity", "_calibrate", "calibrate");
        header("WORLD  --  ?MAPWorldToScreen (0x422380)");
        dumpAt(0x00422380L);
        header("WORLD  --  world coordinate symbols");
        dumpSymbolsMatching("worldtoscreen", "screentoworld", "worldtovp",
                "mapworld", "_mapworld", "_wtoscreen", "wtoscreen");
        header("WORLD  --  viewport and projection symbols");
        dumpSymbolsMatching("_vpinit", "vpinit", "_vpupdate", "vpupdate",
                "_project", "projectpoint", "_unproject", "unproject");
    }
}
