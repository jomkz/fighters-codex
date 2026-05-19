// Consolidates: DumpBRFFunctions, DumpGASFuel, DumpGVProcGAS,
//               DumpGVProcHandlers

public class AnalyzeGAS extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeGAS");
        analyzeGAS();
        closeOutput();
    }
}
