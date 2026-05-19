// Joystick, keyboard, and mouse input handling.
// Dark zone: 0x420000-0x42FFFF (64 KB) -- zero prior coverage.
// Also covers world-coordinate transforms and viewport mapping.
// Invoke: run_ghidra.bat AnalyzeInput.java
// Output: %FA_PROJECT%\output\AnalyzeInput.txt

public class AnalyzeInput extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeInput");
        analyzeInput();
        closeOutput();
    }
}
