import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import java.io.*;
import java.util.*;

public class DumpGASFuel extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\gas_fuel.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // Target: find the routine that reads the GAS `word` capacity field
        // and adds it to the aircraft fuel pool. The word values (108/198/248/315)
        // have no obvious linear mapping to gallons or lbs.
        // Strategy: search FA.SMS for fuel-system symbols (GAS, fuel, tank)
        // then trace the GAS record loader / fuel pool builder.

        // Search for symbols with GAS, fuel, tank, prop in their name.
        out.println("// === FA.SMS symbols matching fuel/gas/tank/prop ===");
        for (Symbol sym : currentProgram.getSymbolTable().getAllSymbols(false)) {
            String name = sym.getName().toLowerCase();
            if (name.contains("fuel") || name.contains("gas") || name.contains("tank")
                    || name.contains("prop")) {
                out.println("// SYM: " + sym.getName() + " @ " + sym.getAddress());
            }
        }
        out.println();

        // Dump all functions whose names contain fuel/gas/tank.
        out.println("// === Functions matching fuel/gas/tank ===");
        Set<Long> dumpedAddrs = new LinkedHashSet<>();
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            String name = f.getName().toLowerCase();
            if (name.contains("fuel") || name.contains("gas") || name.contains("tank")) {
                long ep = f.getEntryPoint().getOffset();
                if (dumpedAddrs.add(ep)) {
                    out.println("// MATCH: " + f.getName() + " @ " + f.getEntryPoint());
                    dumpAt(ep);
                }
            }
        }

        // PT+0xfb is the carrier-approach altitude limit (confirmed); PT fuel fields
        // are nearby. Dump function FUN_00406040 (HUD/aircraft init) which copies
        // the PT struct and may reveal where fuel capacity (GAS word) is stored.
        out.println("// === FUN_00406040 (aircraft/HUD init — PT struct copy) ===");
        dumpAt(0x00406040L);

        // GAS files are loaded by the BRF loader. The PT file references GAS by name.
        // Look for _LoadGAS or any function that reads a GAS record.
        out.println("// === FA.SMS symbols containing 'load' ===");
        for (Symbol sym : currentProgram.getSymbolTable().getAllSymbols(false)) {
            String name = sym.getName();
            if (name.toLowerCase().contains("load") && name.length() < 40) {
                out.println("// SYM: " + name + " @ " + sym.getAddress());
            }
        }

        out.close();
        decompiler.dispose();
        println("Done. Output written to C:\\Temp\\gas_fuel.txt");
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
