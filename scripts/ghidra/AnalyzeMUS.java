// Consolidates: DumpMUSCallbacks, DumpMUSInterpreter, DumpSEQFunctions

public class AnalyzeMUS extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeMUS");
        analyzeMUS();
        closeOutput();
    }
}
