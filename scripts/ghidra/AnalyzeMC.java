// Mission condition DLL (MC) analysis.
// Traces the full condition-check flow from MISSIONInit2 through the
// MC condition dispatcher and all condition-evaluator stubs.
//
// @category FightersAnthology
// @author fighters-toolkit

public class AnalyzeMC extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeMC");

        // MC loader — loads .MC overlay DLL and wires it into the mission engine
        header("MC loader (0x481940)");
        dumpAt(0x00481940L);

        header("Callers of MC loader");
        dumpCallers(0x00481940L);

        // MISSIONInit2 — initialises all mission objects including MC conditions
        header("_MISSIONInit2 (0x480b50)");
        dumpAt(0x00480b50L);

        header("Callers of _MISSIONInit2");
        dumpCallers(0x00480b50L);

        // MM line parser — dispatches .MM keywords; .MC keyword triggers MC load
        header("MM line parser FUN_0047a130");
        dumpAt(0x0047a130L);

        header("MM keyword handler FUN_0047a510");
        dumpAt(0x0047a510L);

        // Mission range around MC init and condition evaluation
        header("Mission range 0x480000-0x483000");
        dumpRange(0x00480000L, 0x00483000L);

        // Additional mission support — used by condition evaluators
        header("Mission range 0x483000-0x486000");
        dumpRange(0x00483000L, 0x00486000L);

        // Object state queries used in conditions (entity alive, in-zone, etc.)
        header("Object state area 0x473000-0x476000");
        dumpRange(0x00473000L, 0x00476000L);

        // Trigger/event subsystem — conditions hook into this
        header("Trigger/event range 0x486000-0x489000");
        dumpRange(0x00486000L, 0x00489000L);

        // String search for MC-related keywords used in .MC DLL exports
        header("String search: MC condition keywords");
        searchStrings(new String[]{".MC", "COND", "cond", "TRIGGER", "trigger",
                "DESTROY", "destroy", "CAPTURE", "capture", "SUCCEED", "FAIL",
                "mczonecheck", "mccheck", "mcevaluate"});

        // Symbol search
        header("Symbols matching mc/mission/cond/trigger/zone/objective/succeed/fail");
        dumpSymbolsMatching("mc", "mission", "cond", "trigger", "zone",
                "objective", "succeed", "fail", "mcinit", "mcproc", "mceval",
                "mccheck", "mczonecheck");

        closeOutput();
    }
}
