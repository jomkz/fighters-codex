// Consolidates: DumpDLGChoosePreload, DumpDLGDispatch, DumpDLGDispatch2,
//               DumpDLGDispatch3, DumpDLGDispatcher, DumpDLGFunctions,
//               DumpDLGHelpers

public class AnalyzeDLG extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeDLG");
        analyzeDLG();
        closeOutput();
    }
}
