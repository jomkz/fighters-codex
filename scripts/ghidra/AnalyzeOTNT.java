// Consolidates: DumpOTNTFlags, DumpOTNTFlags2

public class AnalyzeOTNT extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeOTNT");

        // _GVProc â€” vehicle AI processor
        header("_GVProc (0x473db0)");
        dumpAt(0x00473db0L);

        header("FUN_004bed70");
        dumpAt(0x004bed70L);

        header("FUN_004747c0");
        dumpAt(0x004747c0L);

        // Priority / flag masks â€” large values (OT/NT classification)
        header("Priority mask 0x8000 in 0x400000-0x550000");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x8000L)) dumpAt(va);

        header("Priority mask 0x40000");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x40000L)) dumpAt(va);

        header("Priority mask 0x80000");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x80000L)) dumpAt(va);

        header("Priority mask 0x100000");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x100000L)) dumpAt(va);

        header("Priority mask 0x400000");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x400000L)) dumpAt(va);

        header("Priority mask 0x2000000");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x2000000L)) dumpAt(va);

        header("Priority mask 0x4000000");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x4000000L)) dumpAt(va);

        // Common entity capability masks
        header("Capability mask 0x20 in 0x400000-0x550000");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x20L)) dumpAt(va);

        header("Capability mask 0x100");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x100L)) dumpAt(va);

        header("Capability mask 0x200");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x200L)) dumpAt(va);

        header("Capability mask 0x400");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x400L)) dumpAt(va);

        header("Capability mask 0x800");
        for (long va : findFunctionsWithMask(0x00400000L, 0x00550000L, 0x800L)) dumpAt(va);

        // Hardpoint $2 scan (fire/weapon type discriminator)
        header("Hardpoint $2 scan in 0x460000-0x480000");
        for (long va : findFunctionsWithMask(0x00460000L, 0x00480000L, 0x2L)) dumpAt(va);

        // Symbol search
        header("Symbols matching ot/nt/gv/vehicle/ship/naval/spawn/capability/hardfire");
        dumpSymbolsMatching("gvproc", "gv", "vehicle", "ship", "naval", "spawn",
                "hardfire", "@hardfire", "otnt", "ot_", "nt_");

        closeOutput();
    }
}
