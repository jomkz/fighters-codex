import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import java.io.*;
import java.util.*;

public class DumpHUDWarningBits extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\hud_warningbits.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // FUN_00407930: warning-lights / advisory icon draw function.
        // Reads DAT_0050cfef bits 6-11 to gate each advisory icon.
        // Also reads bits 0-4 (damage) and 29-31 (emergency) — we need to see
        // what display element corresponds to each damage bit.
        out.println("// === FUN_00407930 (warning lights / advisory icons) ===");
        dumpAt(0x00407930L);

        // Find all functions that WRITE to DAT_0050cfef (0x0050cfef).
        // We already know bits 0-13, 28-31. The unknown writer is for bit 14 (0x4000).
        // This will dump all writers; the new one should set the 0x4000 bit.
        out.println("// === All writers to DAT_0050cfef (0x0050cfef) ===");
        Address cfefAddr = toAddr(0x0050cfefL);
        Set<Long> writerAddrs = new LinkedHashSet<>();
        for (ghidra.program.model.symbol.Reference ref :
                currentProgram.getReferenceManager().getReferencesTo(cfefAddr)) {
            if (ref.getReferenceType().isWrite() || ref.getReferenceType().isData()) {
                Function f = currentProgram.getFunctionManager()
                    .getFunctionContaining(ref.getFromAddress());
                if (f != null) writerAddrs.add(f.getEntryPoint().getOffset());
            }
        }
        out.println("// Writers found: " + writerAddrs.size());
        for (long addr : writerAddrs) {
            dumpAt(addr);
        }

        // Also dump FUN_00416380 (autopilot toggle) — likely sets bit 14 when
        // transitioning to carrier-approach flight modes 0x11 / 0x12.
        out.println("// === FUN_00416380 (autopilot / flight-mode toggle) ===");
        dumpAt(0x00416380L);

        out.close();
        decompiler.dispose();
        println("Done. Output written to C:\\Temp\\hud_warningbits.txt");
    }

    private void dumpAt(long address) throws Exception {
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
