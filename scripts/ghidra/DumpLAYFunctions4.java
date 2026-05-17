import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import java.io.*;

public class DumpLAYFunctions4 extends GhidraScript {

    static final long[] ADDRESSES = {
        // hdrPtr writers — the actual LAY DLL data parsers
        0x004B4370L, // writes to hdrPtr; likely ParseLayerFileHeader
        0x004B46D0L, // reads+writes hdrPtr; likely sub-block dispatcher

        // FUN_004b3ad0 — converts LAYER +0x36 byte triplet to colour table pointer
        0x004B3AD0L,
        // FUN_004b3b60 — interpolate int field
        0x004B3B60L,
        // FUN_004b3b80 — interpolate 3-byte entry
        0x004B3B80L,
        // FUN_004b3be0 — called from FUN_004b3480 with DAT_00583940 (LAYER flags)
        0x004B3BE0L,
        // FUN_004b3cb0 — called from FUN_004b3be0 area
        0x004B3CB0L,
        // FUN_004b3d90 — called from FUN_004b3cb0 area
        0x004B3D90L,
        // FUN_004b4170 — just before hdrPtr writer
        0x004B4170L,
        // FUN_004b4680 / 4690 — near second hdrPtr writer
        0x004B4680L,
        0x004B46F0L,
        0x004B4700L,
        0x004B4720L,
    };

    @Override
    public void run() throws Exception {
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\lay_functions4.txt");
        outFile.getParentFile().mkdirs();
        PrintWriter out = new PrintWriter(new FileWriter(outFile));

        for (long addr : ADDRESSES) {
            Address funcAddr = currentProgram.getAddressFactory()
                .getDefaultAddressSpace().getAddress(addr);

            Function func = getFunctionAt(funcAddr);
            if (func == null) func = currentProgram.getFunctionManager().getFunctionContaining(funcAddr);
            if (func == null) {
                DisassembleCommand disCmd = new DisassembleCommand(funcAddr, null, true);
                disCmd.applyTo(currentProgram, monitor);
                CreateFunctionCmd createCmd = new CreateFunctionCmd(funcAddr);
                createCmd.applyTo(currentProgram, monitor);
                func = getFunctionAt(funcAddr);
            }
            if (func != null) {
                DecompileResults results = decompiler.decompileFunction(func, 120, monitor);
                if (results.decompileCompleted()) {
                    out.println("// === " + func.getName() + " @ " + func.getEntryPoint() + " ===");
                    out.println(results.getDecompiledFunction().getC());
                    out.println();
                } else {
                    out.println("// === DECOMPILE FAILED: " + funcAddr + " ===\n");
                }
            } else {
                out.println("// === NO FUNCTION AT: " + funcAddr + " ===\n");
            }
        }

        out.close();
        decompiler.dispose();
        println("Done. Output written to C:\\Temp\\lay_functions4.txt");
    }
}
