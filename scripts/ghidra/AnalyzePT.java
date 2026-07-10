// Consolidates: SetupPT field-assignment helper, PLANE_TYPE struct layout

public class AnalyzePT extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzePT");
        header("PT -- _SetupNT (0x4A7200) [forced]");
        dumpAtForced(0x004A7200L);
        header("PT -- _SetupPT (0x4A7220) [forced]");
        dumpAtForced(0x004A7220L);
        header("PT -- SetupPT wrapper (0x4A71C0) [forced]");
        dumpAtForced(0x004A71C0L);
        header("PT -- _SetupJT (0x4A7230) [forced]");
        dumpAtForced(0x004A7230L);
        header("PT -- _SetupOT (0x4A6EB0) [forced]");
        dumpAtForced(0x004A6EB0L);
        header("PT -- callers of _SetupPT");
        dumpCallers(0x004A7220L);
        header("PT -- callers of _SetupNT");
        dumpCallers(0x004A7200L);
        header("PT -- func_0x004a71e0 [forced]");
        dumpAtForced(0x004A71E0L);
        header("PT -- PT/OT/JT cluster 0x4A6000-0x4A8000");
        dumpRange(0x004A6000L, 0x004A8000L);
        header("PT -- symbols matching setup/plane/pt/ot/jt/brf");
        dumpSymbolsMatching("setup", "plane", "planetype", "pt", "brf", "brfparse");
        closeOutput();
    }
}
