// Campaign DLL binary layout analysis.
// Traces the campaign launcher, mission slot array, and weapon/loadout tables.
// Addresses from FA.SMS / prior RE; campaign binary layout is not yet documented.

public class AnalyzeCAM extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeCAM");
        analyzeCAM();
        closeOutput();
    }
}
