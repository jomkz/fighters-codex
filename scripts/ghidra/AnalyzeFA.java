// Master FA.EXE analysis script.
// Runs every subsystem analysis in sequence into a single output file.
// Invoke: run_ghidra.bat AnalyzeFA.java
// Output: %FA_PROJECT%\output\AnalyzeFA.txt

public class AnalyzeFA extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeFA");

        analyzeLAY();
        analyzeHUD();
        analyzeDLG();
        analyzePROJ();
        analyzeSEE();
        analyzeMM();
        analyzeBI();
        analyzeECM();
        analyzeHGR();
        analyzeMUS();
        analyzeOTNT();
        analyzeT2();
        analyzeGAS();
        analyzeCAM();
        analyzeMC();
        analyzeT2DLL();

        // New subsystem coverage -- dark zones
        analyzeGameLoop();
        analyzeRenderer();
        analyzePhysics();
        analyzeNetwork();
        analyzeInput();

        // BI/AI -- FRAME opcode consumer search
        analyzeBIFRAME();

        closeOutput();
    }
}
