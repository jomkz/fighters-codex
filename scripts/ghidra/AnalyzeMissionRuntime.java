// The mission-runtime tail (#485): the lifecycle around MISSIONTextProc (already read) —
// MISSION init (anti-cheat CRC verification, medal tables), the SingleMission game-mode
// entry, the scoring/stats accumulation, and MISSIONEndScenario. All belong to campaign.
public class AnalyzeMissionRuntime extends FAScript {
    @Override
    public void run() throws Exception {
        openOutput("AnalyzeMissionRuntime");

        header("LIFECYCLE -- anti-cheat (0x4801c0-0x480760)");
        dumpAt(0x004801c0L);  // InitAntiCheat
        dumpAt(0x00480230L);  // ComputeCRC
        dumpAt(0x004804c0L);  // GetAntiCheatIndex
        dumpAt(0x00480530L);  // FindAntiCheatIndex
        dumpAt(0x004805a0L);  // UpdateAntiCheat
        dumpAt(0x00480690L);  // PostAntiCheat
        dumpAt(0x00480700L);  // PostCheatsOn

        header("LIFECYCLE -- MISSIONInit1/2/3 + medals + shutdown");
        dumpAt(0x00480750L);  // _MISSIONInit1 (real)
        dumpAt(0x00480a30L);  // _MISSIONInit2 (real)
        dumpAt(0x00480b40L);  // MISSIONInit1 thunk
        dumpAt(0x00480b60L);  // MISSIONInit3 thunk
        dumpAt(0x00480b80L);  // MISSIONInitMedalInfo
        dumpAt(0x004819f0L);  // MISSIONShutdown

        header("MODE -- SingleMission (0x4a10e0, 1828B) + Create* trampolines");
        dumpAt(0x004a10e0L);
        dumpAt(0x0047faa0L);  // __SingleMission thunk
        dumpAt(0x0047fec0L);  // __CreateQuickMission
        dumpAt(0x0047ff30L);  // __CreateFortMission
        dumpAt(0x0047ff40L);  // __CreateFortMission2

        header("SCORING -- stats accumulators");
        dumpAt(0x004854e0L);  // _WpnStats@28
        dumpAt(0x00485820L);  // _KillStats@12
        dumpAt(0x00485a40L);  // _LandingStats@12
        dumpAt(0x00441c60L);  // _ChooseScoreInit

        header("SCORING -- end of scenario + scoreboard");
        dumpAt(0x00486160L);  // MISSIONEndScenario
        dumpAt(0x00486440L);  // MISSIONScoreSides
        dumpAt(0x004864d0L);  // MISSIONScore
        dumpAt(0x00486500L);  // MISSIONSortPlayers
        dumpAt(0x00486530L);  // FUN_00486530 (sort cmp?)
        dumpAt(0x00486810L);  // MISSIONPrefsChanged
        dumpAt(0x004868b0L);  // MISSIONSucceededForThisPlayer
        dumpAt(0x00486910L);  // MISSIONEnsureLegalName
        dumpAt(0x00486980L);  // FUN_00486980
        dumpAt(0x00486060L);  // CanBackUp

        header("FORT -- setup + UI helpers (0x41f800-0x4210c0)");
        dumpAt(0x0041f800L);
        dumpAt(0x0041f840L);  // FortMultiButtonText
        dumpAt(0x0041f8d0L);
        dumpAt(0x00420790L);
        dumpAt(0x004207c0L);
        dumpAt(0x00420800L);
        dumpAt(0x00420830L);
        dumpAt(0x00420ab0L);
        dumpAt(0x00420ad0L);
        dumpAt(0x00420b20L);
        dumpAt(0x00420bb0L);
        dumpAt(0x0044cf00L);  // FortMultiButton2
        dumpAt(0x0044cf10L);  // FortMultiButtonText2

        closeOutput();
    }
}
