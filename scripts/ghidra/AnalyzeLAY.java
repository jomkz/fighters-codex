// Consolidates: DumpLAYFunctions, DumpLAYFunctions2, DumpLAYFunctions3,
//               DumpLAYFunctions4, DumpLAYGaps, DumpLAYGradient,
//               DumpLAYRemaining, DumpLAYStructure

public class AnalyzeLAY extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeLAY");
        analyzeLAY();
        closeOutput();
    }
}
