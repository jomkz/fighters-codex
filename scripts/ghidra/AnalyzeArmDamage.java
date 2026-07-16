// ArmPlane + the DAMAGE model (#487): the two ends of the combat state machine —
// loadout application (@ArmPlane@4, 11 KB, the largest unclaimed function) and hit
// resolution (_DAMAGEDoHit@12 et al). These are the WRITERS of many still-inferred
// entity fields; reading them promotes the object record from inferred to confirmed.
public class AnalyzeArmDamage extends FAScript {
    @Override
    public void run() throws Exception {
        openOutput("AnalyzeArmDamage");
        header("ARM -- @ArmPlane@4 (0x4197d0, 11115B)");          dumpAt(0x004197d0L);
        header("ARM -- __ArmPlane@4 thunk (0x47fa50)");            dumpAt(0x0047fa50L);
        header("DAMAGE -- _DAMAGEInit@0 (0x40f760)");              dumpAt(0x0040f760L);
        header("DAMAGE -- _DAMAGEInit2@0 (0x40f6b0)");             dumpAt(0x0040f6b0L);
        header("DAMAGE -- _DAMAGEDoHit@12 (0x40f970, 3214B)");     dumpAt(0x0040f970L);
        header("DAMAGE -- _DAMAGEUpdate@0 (0x4108b0, 2512B)");     dumpAt(0x004108b0L);
        header("DAMAGE -- _DAMAGEPorpoise@16 (0x4106d0)");         dumpAt(0x004106d0L);
        header("DAMAGE -- _DAMAGEReport@0 (0x411350)");            dumpAt(0x00411350L);
        header("DAMAGE -- _DAMAGEAutopilotAvail@0 (0x4113c0)");    dumpAt(0x004113c0L);
        header("DAMAGE -- PLANEBreakUp (0x49d730, the destroyed-shape handoff)"); dumpAt(0x0049d730L);
        closeOutput();
    }
}
