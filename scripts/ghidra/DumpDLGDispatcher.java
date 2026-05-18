import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import java.io.*;
import java.util.*;

public class DumpDLGDispatcher extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\dlg_dispatcher.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // FUN_004a6e20: DLG record-params accessor / dispatcher.
        // Decodes the common header gap (+0x02..+0x09) and dispatches
        // per-type draw functions. Also handles _ChoosePreload params.
        out.println("// === FUN_004a6e20 (DLG dispatcher / record-params accessor) ===");
        dumpAt(0x004a6e20L);

        // Dump all callers of FUN_004a6e20 to see how it's invoked
        // and what arguments it receives (the four-i16 _ChoosePreload params).
        out.println("// === Callers of FUN_004a6e20 ===");
        Address dispAddr = toAddr(0x004a6e20L);
        Set<Long> callerAddrs = new LinkedHashSet<>();
        for (Reference ref : currentProgram.getReferenceManager().getReferencesTo(dispAddr)) {
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

        // _ChoosePreload — named symbol if present; also try via SMS-imported label.
        // This is the dialog preload function that takes four i16 params.
        out.println("// === _ChoosePreload (search by name) ===");
        for (Symbol sym : currentProgram.getSymbolTable().getSymbols("_ChoosePreload")) {
            dumpAt(sym.getAddress().getOffset());
        }

        // DLG draw functions — dump remaining ones not in earlier output.
        // _DrawAction +0x16 unknown; _DrawText and _DrawEditBox gaps.
        out.println("// === _DrawAction ===");
        for (Symbol sym : currentProgram.getSymbolTable().getSymbols("_DrawAction")) {
            dumpAt(sym.getAddress().getOffset());
        }
        out.println("// === _DrawText ===");
        for (Symbol sym : currentProgram.getSymbolTable().getSymbols("_DrawText")) {
            dumpAt(sym.getAddress().getOffset());
        }
        out.println("// === _DrawEditBox ===");
        for (Symbol sym : currentProgram.getSymbolTable().getSymbols("_DrawEditBox")) {
            dumpAt(sym.getAddress().getOffset());
        }
        out.println("// === _DrawCampaignList ===");
        for (Symbol sym : currentProgram.getSymbolTable().getSymbols("_DrawCampaignList")) {
            dumpAt(sym.getAddress().getOffset());
        }

        out.close();
        decompiler.dispose();
        println("Done. Output written to C:\\Temp\\dlg_dispatcher.txt");
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
