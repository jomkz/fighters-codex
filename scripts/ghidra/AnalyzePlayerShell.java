// The player-facing shell (#492): the in-flight key command table (FlightKey/KEY*),
// the shell screens (INFO2 reference, FlightMenu, text/format + name-list helpers),
// and the pilot/brief screens that drive campaign.md's state machine.
// Three clusters, three docs: input.md, shell-ui.md, campaign.md.
public class AnalyzePlayerShell extends FAScript {
    @Override
    public void run() throws Exception {
        openOutput("AnalyzePlayerShell");

        // -------- cluster 1: input keys ------------------------------------
        header("KEYS -- ?KEYEvent / KEYECS / KEYLCS / KEYPause (0x411600..)");
        dumpRange(0x00411600L, 0x00411904L);
        header("KEYS -- PutFakeKey / GetFakeKey (0x411f00)");
        dumpRange(0x00411f00L, 0x00411f43L);
        header("KEYS -- _WaitKey (0x412930)");
        dumpAt(0x00412930L);
        header("KEYS -- _SlewKey (0x413d10, 850B)");
        dumpAt(0x00413d10L);
        header("KEYS -- @FlightKey@4 (0x414690, 5724B) THE COMMAND TABLE");
        dumpAt(0x00414690L);
        header("KEYS -- FlightKey statics + autopilot (0x415e30-0x4164af)");
        dumpRange(0x00415e30L, 0x004164afL);
        header("KEYS -- _ServicePlayer@0 (0x4164b0, 3079B)");
        dumpAt(0x004164b0L);
        header("KEYS -- player control init/calibrate/axis keys (0x4170c0-0x418064)");
        dumpRange(0x004170c0L, 0x00418064L);

        // -------- cluster 2: shell-ui --------------------------------------
        header("SHELL -- ShellSetup / ShellOff / MenuShutDown (0x40ba10..)");
        dumpAt(0x0040ba10L);
        dumpAt(0x0040c290L);
        dumpAt(0x0040c310L);
        header("SHELL -- name lists: GetNames / NamesShutdown / FreeNames (0x41c820..)");
        dumpRange(0x0041c820L, 0x0041d739L);
        header("SHELL -- INFO2 reference screens (0x45fec0-0x462603)");
        dumpRange(0x0045fec0L, 0x00462603L);
        header("SHELL -- pic lists: RemapInsignia / MakePicList / FindPic (0x467990..)");
        dumpRange(0x00467990L, 0x00467e2fL);
        header("SHELL -- _FlightMenu (0x474800, 5820B)");
        dumpAt(0x00474800L);
        header("SHELL -- text/format: PageText FormatInit PrepareText PrintText ...");
        dumpAt(0x0047d190L);
        dumpAt(0x0047d1d0L);
        dumpAt(0x0047d440L);
        dumpAt(0x0047ef30L);
        dumpAt(0x0047eff0L);
        dumpAt(0x0047f050L);
        dumpAt(0x0047f500L);
        dumpAt(0x0047f610L);
        header("SHELL -- MakeNamesForList / ScreenDirty / DialogDrawBkgd / MainMenu / SingleFilename");
        dumpAt(0x004a0560L);
        dumpAt(0x004a0610L);
        dumpAt(0x004a0810L);
        dumpAt(0x004a0860L);
        dumpAt(0x004a10b0L);
        header("SHELL -- CampaignSelect / DialogPickFiles / GraphicPrefs (0x4a1810-0x4a2458)");
        dumpRange(0x004a1810L, 0x004a1dcfL);
        dumpAt(0x004a2220L);

        // -------- cluster 3: campaign-pilot --------------------------------
        header("PILOT -- the PILOT record + pilot screen (0x467180-0x4690f5)");
        dumpRange(0x00467180L, 0x004690f5L);
        header("PILOT -- shell->campaign trampolines + Brief paper/map (0x47ff50-0x4801bc)");
        dumpRange(0x0047ff50L, 0x004801bcL);
        header("PILOT -- InitCampaignPilot (0x480c40)");
        dumpAt(0x00480c40L);
        header("PILOT -- campaign proc: LoadCampaignProc/Stores, CampaignMenu, CallCampaignProc");
        dumpAt(0x00480c20L);
        dumpAt(0x00480ea0L);
        dumpRange(0x004811a0L, 0x00481900L);
        header("PILOT -- @BriefScreen@16 (0x4a1dd0, 1075B)");
        dumpAt(0x004a1dd0L);
        header("PILOT -- _ConvertPilotFiles@0 (0x485ae0, 1037B)");
        dumpAt(0x00485ae0L);

        closeOutput();
    }
}
