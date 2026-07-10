// Mission condition DLL (MC) analysis.
// Traces the full condition-check flow from MISSIONInit2 through the
// MC condition dispatcher and all condition-evaluator stubs.

public class AnalyzeMC extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeMC");
        header("MC -- MC loader (0x481940)");
        dumpAt(0x00481940L);
        header("MC -- callers of MC loader");
        dumpCallers(0x00481940L);
        header("MC -- _MISSIONInit2 (0x480b50)");
        dumpAt(0x00480b50L);
        header("MC -- callers of _MISSIONInit2");
        dumpCallers(0x00480b50L);
        header("MC -- MM line parser FUN_0047a130");
        dumpAt(0x0047a130L);
        header("MC -- MM keyword handler FUN_0047a510");
        dumpAt(0x0047a510L);
        header("MC -- mission range 0x480000-0x483000");
        dumpRange(0x00480000L, 0x00483000L);
        header("MC -- mission support 0x483000-0x486000");
        dumpRange(0x00483000L, 0x00486000L);
        header("MC -- object state area 0x473000-0x476000");
        dumpRange(0x00473000L, 0x00476000L);
        header("MC -- trigger/event range 0x486000-0x489000");
        dumpRange(0x00486000L, 0x00489000L);
        header("MC -- string search: MC condition keywords");
        searchStrings(new String[]{".MC", "COND", "cond", "TRIGGER", "trigger",
                "DESTROY", "destroy", "CAPTURE", "capture", "SUCCEED", "FAIL",
                "mczonecheck", "mccheck", "mcevaluate"});
        header("MC -- symbols matching mc/mission/cond/trigger/zone/objective/succeed/fail");
        dumpSymbolsMatching("mc", "mission", "cond", "trigger", "zone",
                "objective", "succeed", "fail", "mcinit", "mcproc", "mceval",
                "mccheck", "mczonecheck");
        closeOutput();
    }
}
