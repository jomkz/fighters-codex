// Targets: PILOT struct gaps 1 (0xB0-0xC1), 3 (0x2018-0x20B7), 4 (0x21F8-0x25DF)
// via PilotSave, stats-flush, and fort-stats functions.
// Gap 2 (0xCF-0x5AE) is variable-length text -- use differential saves instead.

public class AnalyzePLT extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzePLT");
        analyzePLT();
        closeOutput();
    }
}
