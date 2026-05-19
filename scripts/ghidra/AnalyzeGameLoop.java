// Game loop, per-frame object update dispatcher, and initialization sequence.
// Dark zone: 0x468000-0x479DFF (66 KB) -- zero prior coverage.
// Invoke: run_ghidra.bat AnalyzeGameLoop.java
// Output: %FA_PROJECT%\output\AnalyzeGameLoop.txt

public class AnalyzeGameLoop extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeGameLoop");
        analyzeGameLoop();
        closeOutput();
    }
}
