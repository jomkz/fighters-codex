// Consolidates: DumpAIScripts, DumpCTOpcodeArgs, DumpPlaneCheckFuelCT

public class AnalyzeBI extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeBI");
        analyzeBI();
        closeOutput();
    }
}
