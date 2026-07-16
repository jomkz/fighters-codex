// Cockpit sensor model (#486): radar, IR seekers, RWR — "what the player is allowed to
// see". The producer side of the HUD symbology (hud.md) and the input side of the weapons
// seeker/lock model (weapons.md). Start at the per-frame entry points, work out to the RCS.
public class AnalyzeCockpitSensors extends FAScript {
    @Override
    public void run() throws Exception {
        openOutput("AnalyzeCockpitSensors");
        header("SENSOR -- _CPComputeRCS@8 (0x43e8c0) — radar cross-section");   dumpAt(0x0043e8c0L);
        header("SENSOR -- _CPUpdateRadar@0 (0x43e810)");                        dumpAt(0x0043e810L);
        header("SENSOR -- ?CPUpdateIRItems (0x440fe0)");                        dumpAt(0x00440fe0L);
        header("SENSOR -- _CPResetRWR@0 (0x43e830)");                           dumpAt(0x0043e830L);
        header("SENSOR -- @CPAddItemToScopes@4 (0x43dee0)");                    dumpAt(0x0043dee0L);
        header("SENSOR -- @CPRemoveItemFromScopes@4 (0x43e780)");               dumpAt(0x0043e780L);
        header("SENSOR -- @CPRadarRange@4 (0x43ddd0)");                         dumpAt(0x0043ddd0L);
        header("SENSOR -- @CPBombRange@4 (0x43e7e0)");                          dumpAt(0x0043e7e0L);
        header("SENSOR -- ?UsingSuppRadar (0x43de10)");                         dumpAt(0x0043de10L);
        header("SENSOR -- ?CPSetSkill (0x43de90)");                             dumpAt(0x0043de90L);
        header("SENSOR -- _CPInit@0 (0x438b70)");                               dumpAt(0x00438b70L);
        header("SENSOR -- @CPSetMissile@4 (0x438520)");                         dumpAt(0x00438520L);
        header("SENSOR -- the RWR/radar core FUN_0043ea40 (1692B)");            dumpAt(0x0043ea40L);
        header("SENSOR -- the scope-scan FUN_0043df7b (670B)");                 dumpAt(0x0043df7bL);
        closeOutput();
    }
}
