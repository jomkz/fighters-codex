// Consolidates: DumpSEEAndJT, DumpSEETransition, DumpSEETransition2,
//               DumpSEETransition3

public class AnalyzeSEE extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeSEE");

        // Seeker cone / FOV
        header("_PROJInFOV / seeker cone (0x4c2860)");
        dumpAt(0x004c2860L);

        header("Callers of seeker cone (0x4c2860)");
        dumpCallers(0x004c2860L);

        // Search/track lobes
        header("Search lobe (0x4c2eb0)");
        dumpAt(0x004c2eb0L);

        header("Track lobe (0x4c31f0)");
        dumpAt(0x004c31f0L);

        header("Callers of search lobe (0x4c2eb0)");
        dumpCallers(0x004c2eb0L);

        header("Callers of track lobe (0x4c31f0)");
        dumpCallers(0x004c31f0L);

        // Pre-lobe / post-success / guidance
        header("Pre-lobe (0x4c24b0)");
        dumpAt(0x004c24b0L);

        header("Guidance final (0x4c26f0)");
        dumpAt(0x004c26f0L);

        // Lock acquire / target selection
        header("Lock-acquire 1 (0x4c5000)");
        dumpAt(0x004c5000L);

        header("Lock-acquire 2 (0x4c5050)");
        dumpAt(0x004c5050L);

        header("Target selection (0x4c52d0)");
        dumpAt(0x004c52d0L);

        // Seeker state chain
        header("Per-step / _PROJAdd (0x4c0a90)");
        dumpAt(0x004c0a90L);

        header("Missile flag handler (0x4c3eb0)");
        dumpAt(0x004c3eb0L);

        header("Re-eval (0x4c4100)");
        dumpAt(0x004c4100L);

        header("Wrapper (0x4c58a0)");
        dumpAt(0x004c58a0L);

        // Proximity fuze
        header("Proximity fuze (0x4c3960)");
        dumpAt(0x004c3960L);

        // Hit chance
        header("_PROJHitChance@28 (0x4c3380)");
        dumpAt(0x004c3380L);

        // ECM-related (warhead/jammer cross-reference)
        header("ECM+warhead (0x4c2b50)");
        dumpAt(0x004c2b50L);

        header("ECM eval helper (0x4c3360)");
        dumpAt(0x004c3360L);

        header("0x4c20c0");
        dumpAt(0x004c20c0L);

        header("0x4c5670");
        dumpAt(0x004c5670L);

        // Seeker flag scans
        header("Functions with 0x20000 flag in 0x4c0000-0x4c7000");
        for (long va : findFunctionsWithMask(0x004c0000L, 0x004c7000L, 0x20000L)) dumpAt(va);

        header("Functions with 0x100000 flag in 0x4c0000-0x4c7000");
        for (long va : findFunctionsWithMask(0x004c0000L, 0x004c7000L, 0x100000L)) dumpAt(va);

        header("Functions with 0x10000 flag in 0x4c0000-0x4c7000");
        for (long va : findFunctionsWithMask(0x004c0000L, 0x004c7000L, 0x10000L)) dumpAt(va);

        // Symbol search
        header("Symbols matching see/seeker/missile/lock/ir/radar/fov/lobe");
        dumpSymbolsMatching("see", "seeker", "missile", "lock", "ir", "radar", "fov", "lobe",
                "prox", "fuze", "warhead", "guidance");

        closeOutput();
    }
}
