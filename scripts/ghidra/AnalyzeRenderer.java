// 3D rendering pipeline, shape/sprite system, camera and viewport.
// Dark zone: 0x4B4200-0x4BEDFF (106 KB) -- zero prior coverage.
// Also covers shape loader, scene dispatch, and horizon integration.
// Invoke: run_ghidra.bat AnalyzeRenderer.java
// Output: %FA_PROJECT%\output\AnalyzeRenderer.txt

public class AnalyzeRenderer extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeRenderer");
        analyzeRenderer();
        closeOutput();
    }
}
