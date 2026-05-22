// Consolidates: SetupPT field-assignment helper, PLANE_TYPE struct layout

public class AnalyzePT extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzePT");
        analyzePT();
        closeOutput();
    }
}
