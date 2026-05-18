import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import java.io.*;
import java.util.*;

public class DumpLAYGradient extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\lay_gradient.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // LAY format remaining unknowns:
        // 1. Gradient sub-block header bytes 0x31 and 0x10 0x10 — not confirmed yet.
        // 2. sel_alt vs alt scaling — confirm unit.
        // 3. Header gap fields at +0x00/+0x14/+0x18/+0x34..+0x44/+0x60..+0x68.
        // Target: WRFogLayerUpdate (0x004b4320) and LAY record loader.

        out.println("// === WRFogLayerUpdate (0x004b4320) — LAY gradient / atmosphere ===");
        dumpAt(0x004b4320L);

        // Dump callers of WRFogLayerUpdate for context.
        out.println("// === Callers of WRFogLayerUpdate (0x004b4320) ===");
        Address layAddr = toAddr(0x004b4320L);
        Set<Long> callerAddrs = new LinkedHashSet<>();
        for (Reference ref : currentProgram.getReferenceManager().getReferencesTo(layAddr)) {
            if (ref.getReferenceType().isCall()) {
                Function f = currentProgram.getFunctionManager()
                    .getFunctionContaining(ref.getFromAddress());
                if (f != null) callerAddrs.add(f.getEntryPoint().getOffset());
            }
        }
        out.println("// Callers found: " + callerAddrs.size());
        for (long addr : callerAddrs) {
            dumpAt(addr);
        }

        // Search for LAY-related symbols.
        out.println("// === FA.SMS symbols matching LAY/layer/fog/atmosphere/gradient ===");
        for (Symbol sym : currentProgram.getSymbolTable().getAllSymbols(false)) {
            String name = sym.getName().toLowerCase();
            if (name.contains("lay") || name.contains("layer") || name.contains("fog")
                    || name.contains("atmosphere") || name.contains("gradient")
                    || name.contains("horizon") || name.contains("weather")) {
                out.println("// SYM: " + sym.getName() + " @ " + sym.getAddress());
            }
        }
        out.println();

        // Dump all functions with LAY/layer/fog/weather in name.
        out.println("// === Functions matching LAY/layer/fog/weather ===");
        Set<Long> dumpedAddrs = new LinkedHashSet<>();
        dumpedAddrs.add(0x004b4320L); // already dumped
        for (long a : callerAddrs) dumpedAddrs.add(a);
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            String name = f.getName().toLowerCase();
            if (name.contains("lay") || name.contains("layer") || name.contains("fog")
                    || name.contains("atmosphere") || name.contains("gradient")
                    || name.contains("horizon") || name.contains("weather")) {
                long ep = f.getEntryPoint().getOffset();
                if (dumpedAddrs.add(ep)) {
                    out.println("// MATCH: " + f.getName() + " @ " + f.getEntryPoint());
                    dumpAt(ep);
                }
            }
        }

        out.close();
        decompiler.dispose();
        println("Done. Output written to C:\\Temp\\lay_gradient.txt");
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
