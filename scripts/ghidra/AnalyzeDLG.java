// Consolidates: DumpDLGChoosePreload, DumpDLGDispatch, DumpDLGDispatch2,
//               DumpDLGDispatch3, DumpDLGDispatcher, DumpDLGFunctions,
//               DumpDLGHelpers

public class AnalyzeDLG extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeDLG");
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
        closeOutput();
    }
}
