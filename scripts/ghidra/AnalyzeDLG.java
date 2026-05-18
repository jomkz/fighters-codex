// Consolidates: DumpDLGChoosePreload, DumpDLGDispatch, DumpDLGDispatch2,
//               DumpDLGDispatch3, DumpDLGDispatcher, DumpDLGFunctions,
//               DumpDLGHelpers

import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class AnalyzeDLG extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeDLG");

        // Core dialog management
        header("_DialogWhatItem (0x488fc0)");
        dumpAtForced(0x00488fc0L);

        header("_DialogSetup (0x487a63)");
        dumpAt(0x00487a63L);

        header("_DialogShow (0x4880d0)");
        dumpAt(0x004880d0L);

        header("_DialogShutDown (0x488190)");
        dumpAt(0x00488190L);

        header("_DialogDone (0x488300)");
        dumpAt(0x00488300L);

        header("@DialogGetPtr (0x4892e0)");
        dumpAt(0x004892e0L);

        header("@DialogGetValue (0x489300)");
        dumpAt(0x00489300L);

        header("@DialogSelectItem (0x4894f0)");
        dumpAt(0x004894f0L);

        header("@DialogDeselectItem (0x489580)");
        dumpAtForced(0x00489580L);

        header("_TopCenterDialog (0x489710)");
        dumpAtForced(0x00489710L);

        // Preload / choose
        header("_ChoosePreload (0x4897f0)");
        dumpAt(0x004897f0L);

        header("_ChoosePreload helper 0x489830");
        dumpAt(0x00489830L);

        header("_ChoosePreload helper 0x489870");
        dumpAt(0x00489870L);

        header("_ChoosePreload helper 0x4897c0");
        dumpAt(0x004897c0L);

        header("func_0x00489840");
        dumpAt(0x00489840L);

        header("Callers of func_0x00489840");
        dumpCallers(0x00489840L);

        header("_ChooseActivity (0x4a08a0)");
        dumpAt(0x004a08a0L);

        // Draw functions
        header("_DrawText (0x489ac0)");
        dumpAt(0x00489ac0L);

        header("_DrawAction (0x489b90)");
        dumpAt(0x00489b90L);

        header("_DrawFormattedText (0x48a910)");
        dumpAt(0x0048a910L);

        header("_DrawCampaignList (0x48abf0)");
        dumpAt(0x0048abf0L);

        header("_DrawRocker (0x48b4e0)");
        dumpAt(0x0048b4e0L);

        header("_DrawEditBox (0x48c710)");
        dumpAt(0x0048c710L);

        // Dispatcher
        header("FUN_004a6e20 (DLG dispatcher)");
        dumpAt(0x004a6e20L);

        header("Callers of FUN_004a6e20");
        dumpCallers(0x004a6e20L);

        // Additional forced VAs
        header("Force-create cluster 0x488470-0x4a0810");
        dumpAtForced(0x00488470L);
        dumpAtForced(0x00488490L);
        dumpAtForced(0x004a0810L);
        dumpAt(0x0040bd30L);
        dumpAt(0x00436280L);
        dumpAt(0x004891a0L);
        dumpAt(0x00488f50L);
        dumpAt(0x0048d0d0L);
        dumpAt(0x0048d140L);

        // Helpers
        header("FUN_0040d5f0");
        dumpAt(0x0040d5f0L);

        header("FUN_0048b2e0");
        dumpAt(0x0048b2e0L);

        // Callers of _ChoosePreload
        header("Callers of _ChoosePreload (0x4897f0)");
        dumpCallers(0x004897f0L);

        // Bounding-box / hit-test scan
        header("Symbols matching dialog/dlg/choose/screen/panel/button/item");
        dumpSymbolsMatching("dialog", "dlg", "choose", "screen", "panel", "button", "item",
                "preload", "activity", "rocker", "editbox");

        closeOutput();
    }
}
