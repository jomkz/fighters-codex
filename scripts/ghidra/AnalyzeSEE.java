// Consolidates: DumpSEEAndJT, DumpSEETransition, DumpSEETransition2,
//               DumpSEETransition3

public class AnalyzeSEE extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeSEE");
        analyzeSEE();
        closeOutput();
    }
}
