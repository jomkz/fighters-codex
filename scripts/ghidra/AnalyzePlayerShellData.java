// #492 second pass: the data tables and stragglers the first AnalyzePlayerShell pass
// exposed — the kbScanToASCII translation table (the authority for the key-code column
// in input.md), the GetNames globs, INFO2's special-case type names, and the text-engine
// statics that live in the 0x47D190-0x47F660 gap.
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.Memory;

public class AnalyzePlayerShellData extends FAScript {

    private void dumpString(long va, String label) throws Exception {
        Memory mem = currentProgram.getMemory();
        StringBuilder sb = new StringBuilder();
        Address a = toAddr(va);
        for (int i = 0; i < 64; i++) {
            byte b = mem.getByte(a.add(i));
            if (b == 0) break;
            sb.append((b >= 0x20 && b < 0x7f) ? (char) b : '.');
        }
        out.println("// STR 0x" + Long.toHexString(va) + " [" + label + "] = \"" + sb + "\"");
    }

    private void dumpWords(long va, int count, String label) throws Exception {
        Memory mem = currentProgram.getMemory();
        out.println("\n// WORDS 0x" + Long.toHexString(va) + " [" + label + "] count=" + count);
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < count; i++) {
            int w = mem.getShort(toAddr(va + i * 2L)) & 0xffff;
            sb.append(String.format("%04x ", w));
            if (i % 16 == 15) { out.println("// " + String.format("[%02x] ", i - 15) + sb); sb.setLength(0); }
        }
        if (sb.length() > 0) out.println("// [..] " + sb);
    }

    private void dumpDwords(long va, int count, String label) throws Exception {
        Memory mem = currentProgram.getMemory();
        out.println("\n// DWORDS 0x" + Long.toHexString(va) + " [" + label + "] count=" + count);
        for (int i = 0; i < count; i++) {
            int d = mem.getInt(toAddr(va + i * 4L));
            out.println(String.format("//   +%02x: 0x%08x", i * 4, d));
        }
    }

    @Override
    public void run() throws Exception {
        openOutput("AnalyzePlayerShellData");

        header("DATA -- kbScanToASCII (0x4ece00): scan|shift<<7 -> u16 entry");
        dumpWords(0x4ece00L, 256, "kbScanToASCII");
        header("DATA -- __kbScanToASCII byte table at 0x4ecc00 (0x200 bytes as words)");
        dumpWords(0x4ecc00L, 256, "__kbScanToASCII");
        header("DATA -- ignoreRepeats region 0x4ece08 up to waitCounter");
        dumpDwords(0x4ece08L, 16, "ignoreRepeats?");

        header("DATA -- deviceProcs table (0x4ee310, 11 entries)");
        dumpDwords(0x4ee310L, 11, "deviceProcs");

        header("DATA -- GetNames globs + exclusions + misc strings");
        dumpString(0x4eeb48L, "GetNames mask 0x80000 glob");
        dumpString(0x4eeb40L, "GetNames mask 0x4000 glob");
        dumpString(0x4eeb38L, "GetNames mask 0x10000 glob");
        dumpString(0x4eeb34L, "GetNames mask 0x100000 glob / pilot glob");
        dumpString(0x4eea78L, "mission glob ptr target"); dumpDwords(0x4eea78L, 2, "mission ptr pair");
        dumpString(0x4eea68L, "JT glob ptr");  dumpDwords(0x4eea68L, 2, "JT ptr pair");
        dumpString(0x4eea58L, "OT glob ptr");  dumpDwords(0x4eea58L, 2, "OT ptr pair");
        dumpString(0x4eea48L, "NT glob ptr");  dumpDwords(0x4eea48L, 2, "NT ptr pair");
        dumpString(0x4eea38L, "PT glob ptr");  dumpDwords(0x4eea38L, 2, "PT ptr pair");
        dumpString(0x4eeae4L, "GetNames exclusion 1");
        dumpString(0x4eead8L, "GetNames exclusion 2");
        dumpString(0x4eeaccL, "GetNames exclusion 3");
        dumpString(0x4eeac0L, "GetNames exclusion 4");

        header("DATA -- INFO2 strings");
        dumpString(0x4f6e38L, "INF extension");
        dumpString(0x4ec320L, "PIC extension");
        dumpString(0x4eee78L, "INFO2 special type 1"); dumpString(0x4f6e6cL, "-> shape 1");
        dumpString(0x4eee70L, "INFO2 special type 2"); dumpString(0x4f6e60L, "-> shape 2");
        dumpString(0x4f6e58L, "INFO2 special type 3"); dumpString(0x4f6e4cL, "-> shape 3");
        dumpString(0x4eee68L, "INFO2 special type 4"); dumpString(0x4f6e40L, "-> shape 4");
        dumpString(0x4f6ee8L, "Info2 media mode 3 name");
        dumpString(0x4f6ee4L, "Info2 media mode 4 name");
        dumpString(0x4f6ee0L, "Info2 media mode 5 name");
        dumpString(0x4f6edcL, "Info2 media mode 6 name");
        dumpString(0x4f6ef8L, "INFO2SetType compare name");
        dumpString(0x4f6f7cL, "INFO2 fly mission (flyable)");
        dumpString(0x4f6f74L, "INFO2 fly mission (non-flyable)");
        dumpString(0x4f0d98L, "INFO2 fly mission suffix");
        dumpString(0x4f6f8cL, "INFO2 dialog name");

        header("DATA -- pilot/campaign strings");
        dumpString(0x4eb7ccL, "MakePicList special prefix");
        dumpString(0x4ee2c4L, "CallsignChoose insignia ext");
        dumpString(0x4eb914L, "PilotBuildPaper number format");
        dumpString(0x4f7330L, "copy suffix");
        dumpString(0x4fc8f0L, "ConvertPilotFiles rename ext");
        dumpString(0x4fb198L, "CampaignSave RM name");
        dumpString(0x4fb1a8L, "campaign mission name global?");
        dumpString(0x4fb1b8L, "LoadCampaignProc resource");
        dumpString(0x4f8148L, "CampaignSelect text ext");
        dumpString(0x4f6918L, "BriefPaper ext");
        dumpString(0x4fbbd8L, "CreateProMission fixed mission");
        dumpString(0x4f8c7aL, "pilot campaigns-won field (init)");
        dumpString(0x4f0a54L, "campaigns-won separator");

        header("DATA -- ServicePlayer / InitPlayerControl strings");
        dumpString(0x4ee6dcL, "ServicePlayer type-name compare");
        dumpString(0x4ee6e8L, "InitPlayerControl resource");
        dumpString(0x4ee348L, "control config target (first bytes)");

        header("CODE -- SlewKey T_ObjList callback FUN_00414070");
        dumpAt(0x00414070L);
        header("CODE -- text engine: FindSectionHeader / FindSection + statics");
        dumpAt(0x0047d200L);
        dumpAt(0x0047d2c0L);
        dumpAt(0x0047d3a0L);
        dumpAt(0x0047d3c0L);
        dumpAt(0x0047d420L);
        dumpAt(0x0047d6c0L);
        header("CODE -- the token engines FUN_0047d700 (2707B) / FUN_0047e1b0 (3389B)");
        dumpAt(0x0047d700L);
        dumpAt(0x0047e1b0L);
        dumpAt(0x0047eef0L);
        dumpAt(0x0047f61bL);
        header("CODE -- _AddStats@20 thunk (0x4a2a30)");
        dumpAt(0x004a2a30L);

        closeOutput();
    }
}
