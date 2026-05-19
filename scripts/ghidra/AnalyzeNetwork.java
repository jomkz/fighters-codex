// Multiplayer networking protocol, CN_INFO struct mapping, and IP.EXE interface.
// Dark zone: 0x482200-0x4AACEF (169 KB, shared with mission eval) -- zero prior coverage.
// Invoke: run_ghidra.bat AnalyzeNetwork.java
// Output: %FA_PROJECT%\output\AnalyzeNetwork.txt

public class AnalyzeNetwork extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeNetwork");
        analyzeNetwork();
        closeOutput();
    }
}
