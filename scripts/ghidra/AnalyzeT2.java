// Consolidates: DumpT2Loader, DumpT2MMCoords

public class AnalyzeT2 extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeT2");
        analyzeT2();
        closeOutput();
    }
}
