// Mission condition DLL (MC) analysis.
// Traces the full condition-check flow from MISSIONInit2 through the
// MC condition dispatcher and all condition-evaluator stubs.

public class AnalyzeMC extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeMC");
        analyzeMC();
        closeOutput();
    }
}
