import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.program.model.symbol.*;
import ghidra.program.model.pcode.*;
import java.io.*;
import java.util.*;

public class DumpLAYFunctions3 extends GhidraScript {

    @Override
    public void run() throws Exception {
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\lay_functions3.txt");
        outFile.getParentFile().mkdirs();
        PrintWriter out = new PrintWriter(new FileWriter(outFile));

        // Find cross-references to hdrPtr (0x00580D94) — written by the LAY loader
        Address hdrPtrAddr = currentProgram.getAddressFactory()
            .getDefaultAddressSpace().getAddress(0x00580D94L);
        out.println("// === Cross-references to hdrPtr (0x00580D94) ===");
        currentProgram.getReferenceManager().getReferencesTo(hdrPtrAddr).forEach(ref -> {
            out.println("//   " + ref.getReferenceType() + " from " + ref.getFromAddress());
        });
        out.println();

        // Find cross-references to curLayers (0x00583250) — written by the LAY loader
        Address curLayersAddr = currentProgram.getAddressFactory()
            .getDefaultAddressSpace().getAddress(0x00583250L);
        out.println("// === Cross-references to curLayers (0x00583250) ===");
        currentProgram.getReferenceManager().getReferencesTo(curLayersAddr).forEach(ref -> {
            out.println("//   " + ref.getReferenceType() + " from " + ref.getFromAddress());
        });
        out.println();

        // Dump functions in unexplored 0x4B3000-0x4B3800 range
        long[] addrs = {
            0x004B3010L,
            0x004B3170L,
            0x004B31F0L,
            0x004B3410L,
            0x004B3480L,
            0x004B3750L,
            0x004B3820L,
            0x004B382AL,
            0x004B3AD0L,
            0x004B3B60L,
            0x004B3B80L,
            0x004B3BE0L,
            0x004B3CB0L,
            0x004B3D90L,
            0x004B4170L,
            // Horizon sub-functions
            0x004C9066L,
            0x004C90F4L,
            0x004C91ADL,
            0x004C9179L,
        };

        for (long addr : addrs) {
            Address funcAddr = currentProgram.getAddressFactory()
                .getDefaultAddressSpace().getAddress(addr);
            dumpAt(funcAddr, decompiler, out);
        }

        out.close();
        decompiler.dispose();
        println("Done. Output written to C:\\Temp\\lay_functions3.txt");
    }

    private void dumpAt(Address funcAddr, DecompInterface decompiler, PrintWriter out) throws Exception {
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
