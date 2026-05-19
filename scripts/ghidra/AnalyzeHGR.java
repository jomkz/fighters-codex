// Consolidates: DumpHGRFormat, DumpHGRLoader, DumpHGRT2Bit14

public class AnalyzeHGR extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeHGR");
        analyzeHGR();
        closeOutput();
    }
}
