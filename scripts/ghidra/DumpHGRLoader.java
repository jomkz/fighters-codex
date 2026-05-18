import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import java.io.*;
import java.util.*;

public class DumpHGRLoader extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\hgr_loader.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // HGR hangar layout format.
        // Two files: H_AIRB.HGR (land base) and H_AIRB2.HGR (carrier/alternate).
        // Target: find the HGR loader in FA.EXE — extract hangar layout table,
        // aircraft slot positions, icon placement, camera angle.

        // Search for HGR-related symbols.
        out.println("// === FA.SMS symbols matching HGR/hangar/hanger/H_AIRB ===");
        for (Symbol sym : currentProgram.getSymbolTable().getAllSymbols(false)) {
            String name = sym.getName().toLowerCase();
            if (name.contains("hgr") || name.contains("hangar") || name.contains("hanger")
                    || name.contains("h_airb") || name.contains("airbase")) {
                out.println("// SYM: " + sym.getName() + " @ " + sym.getAddress());
            }
        }
        out.println();

        // Dump all functions with HGR/hangar in name.
        out.println("// === Functions matching HGR/hangar ===");
        Set<Long> dumpedAddrs = new LinkedHashSet<>();
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            String name = f.getName().toLowerCase();
            if (name.contains("hgr") || name.contains("hangar") || name.contains("hanger")) {
                long ep = f.getEntryPoint().getOffset();
                if (dumpedAddrs.add(ep)) {
                    out.println("// MATCH: " + f.getName() + " @ " + f.getEntryPoint());
                    dumpAt(ep);
                }
            }
        }

        // HGR files are loaded as Win32 PE DLLs (like HUD files).
        // FUN_00406040 loads HUD DLLs; look for a parallel function for HGR.
        // Also search for "H_AIRB" string references.
        out.println("// === String/data references to H_AIRB ===");
        for (Symbol sym : currentProgram.getSymbolTable().getAllSymbols(false)) {
            String name = sym.getName().toUpperCase();
            if (name.contains("H_AIRB") || name.equals("HGR")) {
                out.println("// SYM: " + sym.getName() + " @ " + sym.getAddress());
                for (Reference ref : currentProgram.getReferenceManager()
                        .getReferencesTo(sym.getAddress())) {
                    Function f2 = currentProgram.getFunctionManager()
                        .getFunctionContaining(ref.getFromAddress());
                    if (f2 != null && dumpedAddrs.add(f2.getEntryPoint().getOffset())) {
                        out.println("// REFERENCED FROM: " + f2.getName() + " @ " + f2.getEntryPoint());
                        dumpAt(f2.getEntryPoint().getOffset());
                    }
                }
            }
        }

        // Hangar / debriefing screen functions — likely near HUD init (FUN_00406040).
        // Dump functions in the 0x0040_6000 – 0x0040_8000 range that are not yet
        // categorised, to find the HGR DLL loader.
        out.println("// === Functions near FUN_00406040 (0x406000–406200) ===");
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            long ep = f.getEntryPoint().getOffset();
            if (ep >= 0x00406000L && ep <= 0x00406200L && dumpedAddrs.add(ep)) {
                dumpAt(ep);
            }
        }

        out.close();
        decompiler.dispose();
        println("Done. Output written to C:\\Temp\\hgr_loader.txt");
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
