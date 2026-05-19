// Consolidates: DumpOTNTFlags, DumpOTNTFlags2

public class AnalyzeOTNT extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeOTNT");
        analyzeOTNT();
        closeOutput();
    }
}
