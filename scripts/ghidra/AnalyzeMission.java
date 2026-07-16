// Mission runtime (#485): the .M/.MT interpreter, mission lifecycle, Fort/Quick/Single
// mission construction, scoring and end-of-mission stats. These are the consumers of the
// .M / .MT format specs — the interpreter that runs a mission file, never before traced.

public class AnalyzeMission extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeMission");

        header("MISSION -- the interpreter: _MISSIONTextProc@16 (0x481c10, 8291B)");
        dumpAt(0x00481c10L);
        header("MISSION -- _CallMissionProc@8 (0x481940) — the proc dispatcher");
        dumpAt(0x00481940L);

        header("MISSION -- lifecycle: MISSIONLoad (0x428412)");
        dumpAt(0x00428412L);
        header("MISSION -- _MISSIONInit1@@ (0x480750)");
        dumpAt(0x00480750L);
        header("MISSION -- _MISSIONInit2@@ (0x480a30)");
        dumpAt(0x00480a30L);
        header("MISSION -- _MISSIONLoadCommonResources@0 (0x486010)");
        dumpAt(0x00486010L);
        header("MISSION -- MISSIONLoadOrdIcons (0x4809d0)");
        dumpAt(0x004809d0L);
        header("MISSION -- _MISSIONSetCheating@0 (0x480be0)");
        dumpAt(0x00480be0L);
        header("MISSION -- _MISSIONCheckSuccess@0 (0x486860)");
        dumpAt(0x00486860L);

        header("MISSION -- scoring: _MISSIONAddScore@12 (0x486580)");
        dumpAt(0x00486580L);
        header("MISSION -- _EndOfMissionStats@0 (0x484d90)");
        dumpAt(0x00484d90L);
        header("MISSION -- _EndOfFortMissionStats@0 (0x485040)");
        dumpAt(0x00485040L);
        header("MISSION -- _MISSIONFortDestroyed@4 (0x4851c0)");
        dumpAt(0x004851c0L);
        header("MISSION -- _MISSIONFortDestroyedByFort@4 (0x485260)");
        dumpAt(0x00485260L);
        header("MISSION -- _MISSIONFortStatus@4 (0x4852f0)");
        dumpAt(0x004852f0L);
        header("MISSION -- MISSIONFortWin (0x4860f0)");
        dumpAt(0x004860f0L);

        header("MISSION -- construction: __SingleMission@0 (0x47faa0)");
        dumpAt(0x0047faa0L);
        header("MISSION -- __CreateQuickMission@4 (0x47fec0)");
        dumpAt(0x0047fec0L);
        header("MISSION -- __CreateFortMission@4 (0x47ff30)");
        dumpAt(0x0047ff30L);
        header("MISSION -- _QuickMission@4 (0x42e9a0)");
        dumpAt(0x0042e9a0L);
        header("MISSION -- _FortMission@4 (0x41fb60)");
        dumpAt(0x0041fb60L);
        header("MISSION -- _FortMission2@4 (0x44d070)");
        dumpAt(0x0044d070L);

        header("MISSION -- Fort loadout: _HARDSaveFortLoads@0 (0x453d50)");
        dumpAt(0x00453d50L);
        header("MISSION -- _HARDRestoreFortLoad@0 (0x453ec0)");
        dumpAt(0x00453ec0L);
        header("MISSION -- _HARDRearmFortTest@0 (0x453f90)");
        dumpAt(0x00453f90L);
        header("MISSION -- _HARDRearmFortLoad@0 (0x454060)");
        dumpAt(0x00454060L);

        closeOutput();
    }
}
