// AI/BI opcode 0x28 (FRAME) consumer analysis.
//
// The FRAME opcode (case 0x28 in the BI bytecode dispatch switch) writes two s16
// values into the CT interpreter state block at +0x7c/+0x7e (DAT_00546c44/46).
// The writer is confirmed (FUN_00466a80, DumpAllFunctions.txt line 78118).
// Access is via pointer (*(DAT_0050cf90) + 0x7c) -- invisible to static offset scans.
// This script finds consumers via cross-reference to DAT_00546bc8 (CT state base).

public class AnalyzeBIFRAME extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeBIFRAME");
        analyzeBIFRAME();
        closeOutput();
    }
}
