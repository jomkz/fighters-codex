// Flight model, aerodynamics, and collision detection.
// Dark zone: 0x4D0000-0x4EFFFF (128 KB) -- zero prior coverage.
// Also resolves _PROJProc virtual dispatch and terrain collision path.
// Invoke: run_ghidra.bat AnalyzePhysics.java
// Output: %FA_PROJECT%\output\AnalyzePhysics.txt

public class AnalyzePhysics extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzePhysics");
        analyzePhysics();
        closeOutput();
    }
}
