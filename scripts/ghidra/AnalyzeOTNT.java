// Consolidates: DumpOTNTFlags, DumpOTNTFlags2

public class AnalyzeOTNT extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeOTNT");
        header("OTNT -- _GVProc (0x473db0)");
        dumpAt(0x00473db0L);
        header("OTNT -- FUN_004bed70");
        dumpAt(0x004bed70L);
        header("OTNT -- FUN_004747c0");
        dumpAt(0x004747c0L);
        header("OTNT -- priority mask 0x8000");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x8000L)) dumpAt(va);
        header("OTNT -- priority mask 0x40000");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x40000L)) dumpAt(va);
        header("OTNT -- priority mask 0x80000");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x80000L)) dumpAt(va);
        header("OTNT -- priority mask 0x100000");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x100000L)) dumpAt(va);
        header("OTNT -- priority mask 0x400000");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x400000L)) dumpAt(va);
        header("OTNT -- priority mask 0x2000000");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x2000000L)) dumpAt(va);
        header("OTNT -- priority mask 0x4000000");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x4000000L)) dumpAt(va);
        header("OTNT -- capability mask 0x20");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x20L)) dumpAt(va);
        header("OTNT -- capability mask 0x100");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x100L)) dumpAt(va);
        header("OTNT -- capability mask 0x200");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x200L)) dumpAt(va);
        header("OTNT -- capability mask 0x400");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x400L)) dumpAt(va);
        header("OTNT -- capability mask 0x800");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x800L)) dumpAt(va);
        header("OTNT -- hardpoint $2 scan 0x460000-0x480000");
        for (long va : findFunctionsWithMask(0x00460000L, 0x00480000L, 0x2L)) dumpAt(va);
        header("OTNT -- ot_flags bit 17 (0x20000) scan");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x20000L)) dumpAt(va);
        header("OTNT -- ot_flags bit 21 (0x200000) scan");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x200000L)) dumpAt(va);
        header("OTNT -- NT flag bits 18-20 scan GV-range 0x470000-0x480000");
        for (long m : new long[]{0x40000L, 0x80000L, 0x100000L})
            for (long va : findFunctionsWithMask(0x00470000L, 0x00480000L, m)) dumpAt(va);
        header("OTNT -- NT flag bits 25-26 scan GV-range");
        for (long m : new long[]{0x2000000L, 0x4000000L})
            for (long va : findFunctionsWithMask(0x00470000L, 0x00480000L, m)) dumpAt(va);
        header("OTNT -- NT bit 10 (0x400) narrowed to 0x470000-0x480000");
        for (long va : findFunctionsWithMask(0x00470000L, 0x00480000L, 0x400L)) dumpAt(va);
        header("OTNT -- symbols matching ot/nt/gv/vehicle/ship/naval/spawn/capability/hardfire");
        dumpSymbolsMatching("gvproc", "gv", "vehicle", "ship", "naval", "spawn",
                "hardfire", "@hardfire", "otnt", "ot_", "nt_", "civilian", "dual");
        closeOutput();
    }
}
