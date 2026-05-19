// Consolidates: DumpBit14Targeted, DumpHUDBit14, DumpHUDBit14Search,
//               DumpHUDFunctions, DumpHUDGap, DumpHUDHVel,
//               DumpHUDLoader, DumpHUDWarningBits, DumpHUDWarningBits2

public class AnalyzeHUD extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeHUD");
        analyzeHUD();
        closeOutput();
    }
}
