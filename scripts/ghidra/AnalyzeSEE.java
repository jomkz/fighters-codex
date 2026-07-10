// Consolidates: DumpSEEAndJT, DumpSEETransition, DumpSEETransition2,
//               DumpSEETransition3

public class AnalyzeSEE extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeSEE");
        header("SEE -- _PROJInFOV / seeker cone (0x4c2860)");
        dumpAt(0x004c2860L);
        header("SEE -- callers of seeker cone");
        dumpCallers(0x004c2860L);
        header("SEE -- search lobe (0x4c2eb0)");
        dumpAt(0x004c2eb0L);
        header("SEE -- track lobe (0x4c31f0)");
        dumpAt(0x004c31f0L);
        header("SEE -- callers of search lobe");
        dumpCallers(0x004c2eb0L);
        header("SEE -- callers of track lobe");
        dumpCallers(0x004c31f0L);
        header("SEE -- pre-lobe (0x4c24b0)");
        dumpAt(0x004c24b0L);
        header("SEE -- guidance final (0x4c26f0)");
        dumpAt(0x004c26f0L);
        header("SEE -- lock-acquire 1 (0x4c5000)");
        dumpAt(0x004c5000L);
        header("SEE -- lock-acquire 2 (0x4c5050)");
        dumpAt(0x004c5050L);
        header("SEE -- target selection (0x4c52d0)");
        dumpAt(0x004c52d0L);
        header("SEE -- _PROJAdd (0x4c0a90)");
        dumpAt(0x004c0a90L);
        header("SEE -- missile flag handler (0x4c3eb0)");
        dumpAt(0x004c3eb0L);
        header("SEE -- re-eval (0x4c4100)");
        dumpAt(0x004c4100L);
        header("SEE -- wrapper (0x4c58a0)");
        dumpAt(0x004c58a0L);
        header("SEE -- proximity fuze (0x4c3960)");
        dumpAt(0x004c3960L);
        header("SEE -- _PROJHitChance@28 (0x4c3380)");
        dumpAt(0x004c3380L);
        header("SEE -- ECM+warhead cluster");
        dumpAt(0x004c2b50L); dumpAt(0x004c3360L); dumpAt(0x004c20c0L); dumpAt(0x004c5670L);
        header("SEE -- 0x20000 flag scan 0x4c0000-0x4c7000");
        for (long va : findFunctionsWithMask(0x004c0000L, 0x004c7000L, 0x20000L)) dumpAt(va);
        header("SEE -- 0x100000 flag scan");
        for (long va : findFunctionsWithMask(0x004c0000L, 0x004c7000L, 0x100000L)) dumpAt(va);
        header("SEE -- 0x10000 flag scan");
        for (long va : findFunctionsWithMask(0x004c0000L, 0x004c7000L, 0x10000L)) dumpAt(va);
        header("SEE -- symbols matching see/seeker/missile/lock/ir/radar/fov/lobe");
        dumpSymbolsMatching("see", "seeker", "missile", "lock", "ir", "radar", "fov", "lobe",
                "prox", "fuze", "warhead", "guidance");
        closeOutput();
    }
}
