import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import java.io.*;

public class DumpSEEAndJT extends GhidraScript {

    @Override
    public void run() throws Exception {
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\see_jt.txt");
        outFile.getParentFile().mkdirs();
        PrintWriter out = new PrintWriter(new FileWriter(outFile));

        // FUN_004c2eb0: SEE search-lobe check.
        // Called when missile+0xa6 & 0x10000 (search mode).
        // Expected to write missile+0xa6 |= 0x20000 on successful acquisition.
        out.println("// === FUN_004c2eb0 (SEE search lobe check) ===");
        dumpAt(0x004c2eb0L, decompiler, out);

        // FUN_004c31f0: SEE track-lobe check.
        // Called when missile+0xa6 & 0x20000 (track mode).
        // Requires target+0xde & 0x100000. May also set that flag.
        out.println("// === FUN_004c31f0 (SEE track lobe check) ===");
        dumpAt(0x004c31f0L, decompiler, out);

        // FUN_004c3380: _PROJHitChance@28.
        // Already known to read ECM+0x12/+0x17 for Pk reduction.
        // May also read JT warhead flags (bits 0-15) for fuze type or hit model.
        out.println("// === FUN_004c3380 (_PROJHitChance@28) ===");
        dumpAt(0x004c3380L, decompiler, out);

        // FUN_004c3960: Fuze Pk evaluation (from script comments in prior session).
        // Expected to read JT warhead dword for proximity fuze / blast radius flags.
        out.println("// === FUN_004c3960 (fuze/Pk eval) ===");
        dumpAt(0x004c3960L, decompiler, out);

        // FUN_004c2860: _PROJInFOV@40.
        // Performs range/heading check using lobe data.
        // Already partially known; decompile to see full logic and any warhead reads.
        out.println("// === FUN_004c2860 (_PROJInFOV@40) ===");
        dumpAt(0x004c2860L, decompiler, out);

        out.close();
        decompiler.dispose();
        println("Done. Output written to C:\\Temp\\see_jt.txt");
    }

    private void dumpAt(long address, DecompInterface decompiler, PrintWriter out) throws Exception {
        Address funcAddr = currentProgram.getAddressFactory()
            .getDefaultAddressSpace().getAddress(address);
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
}
