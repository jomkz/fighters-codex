import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class DumpAIScripts extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;
    private Set<Long> dumped = new LinkedHashSet<>();

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\ai_scripts.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // Goal: confirm move/jink <speed_mode> and <value> semantics, and
        // decode the .BI bytecode format (opcode table, argument encoding).
        //
        // Entry points confirmed from FA.SMS:
        //   _CTDo_move    @ 0x00465CC0
        //   _CTDo_movetoalt @ 0x00465E20
        //   _CTDo_jink    @ 0x004663F0
        //   _CreateMove@52  @ 0x00463A20
        //   _CreateMoveGoal@20 @ 0x00463AF0
        //   @WriteCmdBufMove@4 @ 0x00463CC0

        out.println("// === _CTDo_move @ 0x00465CC0 ===");
        dumpAt(0x00465CC0L);

        out.println("\n// === _CTDo_movetoalt @ 0x00465E20 ===");
        dumpAt(0x00465E20L);

        out.println("\n// === _CTDo_jink @ 0x004663F0 ===");
        dumpAt(0x004663F0L);

        out.println("\n// === _CreateMove@52 @ 0x00463A20 ===");
        dumpAt(0x00463A20L);

        out.println("\n// === _CreateMoveGoal@20 @ 0x00463AF0 ===");
        dumpAt(0x00463AF0L);

        out.println("\n// === @WriteCmdBufMove@4 @ 0x00463CC0 ===");
        dumpAt(0x00463CC0L);

        // .BI bytecode: search FA.SMS for interpreter / opcode dispatch symbols.
        // Also look for "_MISSIONInit2@0" which loads the mission AI state.
        out.println("\n// === _MISSIONInit2@0 @ 0x00480B50 ===");
        dumpAt(0x00480B50L);

        // Dump callers of _CTDo_move and _CTDo_jink to find the AI dispatch loop.
        out.println("\n// === Callers of _CTDo_move (0x00465CC0) ===");
        dumpCallers(0x00465CC0L);

        out.println("\n// === Callers of _CTDo_jink (0x004663F0) ===");
        dumpCallers(0x004663F0L);

        // Search for .BI / bytecode interpreter: look for functions with 'interp', 'opcode', 'script'
        out.println("\n// === FA.SMS symbols matching script/interp/opcode/BI ===");
        for (Symbol sym : currentProgram.getSymbolTable().getAllSymbols(false)) {
            String name = sym.getName().toLowerCase();
            if (name.contains("script") || name.contains("interp") || name.contains("opcode")
                    || name.contains("bytecode") || name.contains("_bi") || name.contains("ctdo_")) {
                out.println("// SYM: " + sym.getName() + " @ " + sym.getAddress());
            }
        }

        // Dump remaining CTDo_ functions which implement each AI script command.
        out.println("\n// === All _CTDo_* functions ===");
        for (Symbol sym : currentProgram.getSymbolTable().getAllSymbols(false)) {
            String name = sym.getName().toLowerCase();
            if (name.startsWith("_ctdo_") || name.startsWith("ctdo_")) {
                out.println("// SYM: " + sym.getName() + " @ " + sym.getAddress());
                Function fn = currentProgram.getFunctionManager().getFunctionAt(sym.getAddress());
                if (fn != null) dumpAt(fn.getEntryPoint().getOffset());
            }
        }

        out.close();
        decompiler.dispose();
        println("Output: C:\\Temp\\ai_scripts.txt");
    }

    private void dumpCallers(long targetVA) throws Exception {
        Address targetAddr = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(targetVA);
        for (Reference ref : currentProgram.getReferenceManager().getReferencesTo(targetAddr)) {
            if (ref.getReferenceType().isCall()) {
                Function fn = currentProgram.getFunctionManager()
                        .getFunctionContaining(ref.getFromAddress());
                if (fn != null) dumpAt(fn.getEntryPoint().getOffset());
            }
        }
    }

    private void dumpAt(long va) throws Exception {
        if (dumped.contains(va)) return;
        dumped.add(va);
        Address addr = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(va);
        Function fn = currentProgram.getFunctionManager().getFunctionAt(addr);
        if (fn == null) fn = currentProgram.getFunctionManager().getFunctionContaining(addr);
        if (fn == null) {
            out.println("// NOT FOUND at 0x" + Long.toHexString(va));
            return;
        }
        out.println("// --- " + fn.getName() + " @ 0x" + Long.toHexString(va) + " ---");
        DecompileResults res = decompiler.decompileFunction(fn, 120, monitor);
        if (res != null && res.getDecompiledFunction() != null) {
            out.println(res.getDecompiledFunction().getC());
        } else {
            out.println("// decompile failed: " + fn.getName());
        }
        out.println();
    }
}
