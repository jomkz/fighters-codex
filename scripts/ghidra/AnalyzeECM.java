// Consolidates: DumpECMEval, DumpECMGeometry, DumpECMPower

public class AnalyzeECM extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeECM");
        analyzeECM();
        closeOutput();
    }
}
