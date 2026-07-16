// The cockpit-sensor tail (#486): the FUN_-only detection-test predicates, RWR spike
// timing, scope-scan, and CPDraw scope rendering that the CP* core (RCS + scopes, #520)
// dispatches into. Dump CPAddItemToScopes first to see which predicates it calls, then
// the whole unclaimed cluster.
public class AnalyzeSensorThresholds extends FAScript {
    @Override
    public void run() throws Exception {
        openOutput("AnalyzeSensorThresholds");

        header("GATE -- CPAddItemToScopes (0x43dee0) — the 3 detection predicates it calls");
        dumpAt(0x0043dee0L);

        header("RWR -- the RWR/radar core FUN_0043ea40 (1692B)");
        dumpAt(0x0043ea40L);
        header("SCAN -- scope-scan FUN_0043df7b (670B) + neighbours");
        dumpAt(0x0043df70L);
        dumpAt(0x0043df7bL);
        dumpAt(0x0043e220L);
        dumpAt(0x0043e330L);
        dumpAt(0x0043e450L);
        dumpAt(0x0043e700L);
        dumpAt(0x0043e7b0L);

        header("DETECT -- threshold helpers (0x43bb50-0x43dcc0)");
        dumpAt(0x0043bb50L);
        dumpAt(0x0043bba0L);
        dumpAt(0x0043bf60L);
        dumpAt(0x0043c2f0L);
        dumpAt(0x0043c5b0L);
        dumpAt(0x0043c610L);
        dumpAt(0x0043cec0L);
        dumpAt(0x0043cf10L);
        dumpAt(0x0043cfb0L);
        dumpAt(0x0043d0e0L);
        dumpAt(0x0043d290L);
        dumpAt(0x0043d460L);
        dumpAt(0x0043d690L);
        dumpAt(0x0043d69bL);
        dumpAt(0x0043db40L);
        dumpAt(0x0043dcc0L);

        header("AI -- FUN_0043a5c0 (5499B, the WRCanSee-gated AI detection scan) + neighbours");
        dumpAt(0x00439e40L);
        dumpAt(0x0043a0b0L);
        dumpAt(0x0043a190L);
        dumpAt(0x0043a364L);
        dumpAt(0x0043a400L);
        dumpAt(0x0043a5c0L);

        header("SCOPE-DRAW -- the big FUN_ scope renderers (0x43c6b0, 0x43f0ea, 0x43f51a)");
        dumpAt(0x0043c6b0L);
        dumpAt(0x0043f0e0L);
        dumpAt(0x0043f0eaL);
        dumpAt(0x0043f280L);
        dumpAt(0x0043f300L);
        dumpAt(0x0043f360L);
        dumpAt(0x0043f510L);
        dumpAt(0x0043f51aL);
        dumpAt(0x00440bf0L);
        dumpAt(0x00440d00L);

        header("MISC -- WPCInit (0x438870) + FUN_0043887b + CPNextTarget (0x440e10)");
        dumpAt(0x00438870L);
        dumpAt(0x0043887bL);
        dumpAt(0x00440e10L);

        closeOutput();
    }
}
