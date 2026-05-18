import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import java.io.*;
import java.util.*;

public class DumpPlaneCheckFuelCT extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;
    private Set<Long> dumped = new LinkedHashSet<>();

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\planecheckfuel_ct.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // Goal 1: _PLANECheckFuel@0 (0x49fb70) — returns bitmask OR'd into DAT_0050cfef.
        // Bits 15-18 confirmed as carrier glideslope. Bit 14 source unknown — is it
        // another glideslope state, a low-fuel advisory, or something else?
        out.println("// === _PLANECheckFuel@0 (0x49fb70) ===");
        dumpAt(0x0049fb70L);

        // Goal 2: _CTExecProgram@4 (0x466970) — the BI bytecode interpreter.
        // This dispatches to CTDo_* and CTEval_* handlers. Need to see how it reads
        // the opcode/argument encoding so we can document the BI binary format.
        out.println("\n// === _CTExecProgram@4 (0x466970) ===");
        dumpAt(0x00466970L);

        // Goal 3: _CTDo_move + _CTDo_jink — already force-created; re-dump with context.
        // Also dump _CTDo_maneuver, _CTDo_turn, _CTDo_rudder for comparison.
        out.println("\n// === _CTDo_move (0x465cc0) ===");
        dumpAtForced(0x00465cc0L);

        out.println("\n// === _CTDo_jink (0x4663f0) ===");
        dumpAtForced(0x004663f0L);

        out.println("\n// === _CTDo_maneuver (0x465a70) ===");
        dumpAtForced(0x00465a70L);

        out.println("\n// === _CTDo_turn (0x465ea0) ===");
        dumpAtForced(0x00465ea0L);

        // Goal 4: _MVRJink@40 (0x4ac9e0) — the actual jink maneuver executor.
        // Will tell us what the jink angle/speed args mean.
        out.println("\n// === _MVRJink@40 (0x4ac9e0) ===");
        dumpAt(0x004ac9e0L);

        // Goal 5: callers of _CTExecProgram@4 — find the AI update loop.
        out.println("\n// === Callers of _CTExecProgram@4 (0x466970) ===");
        Address ctExecAddr = toAddr(0x00466970L);
        Set<Long> callers = new LinkedHashSet<>();
        for (Reference ref : currentProgram.getReferenceManager().getReferencesTo(ctExecAddr)) {
            if (ref.getReferenceType().isCall()) {
                Function fn = currentProgram.getFunctionManager()
                        .getFunctionContaining(ref.getFromAddress());
                if (fn != null && callers.add(fn.getEntryPoint().getOffset())) {
                    out.println("// CALLER: " + fn.getName() + " @ " + fn.getEntryPoint());
                }
            }
        }
        for (long va : callers) dumpAt(va);

        // Goal 6: functions adjacent to CTExecProgram (0x466800-0x466a00) —
        // may contain the opcode table or dispatch index.
        out.println("\n// === Functions near _CTExecProgram (0x466800-0x466a00) ===");
        for (Function fn : currentProgram.getFunctionManager().getFunctions(true)) {
            long ep = fn.getEntryPoint().getOffset();
            if (ep >= 0x00466800L && ep <= 0x00466a00L && !dumped.contains(ep)) {
                dumpAt(ep);
            }
        }

        out.close();
        decompiler.dispose();
        println("Output: C:\\Temp\\planecheckfuel_ct.txt");
    }

    private void dumpAtForced(long va) throws Exception {
        if (dumped.contains(va)) return;
        dumped.add(va);
        Address addr = toAddr(va);
        Function fn = currentProgram.getFunctionManager().getFunctionAt(addr);
        if (fn == null) {
            DisassembleCommand disCmd = new DisassembleCommand(addr, null, true);
            disCmd.applyTo(currentProgram, monitor);
            CreateFunctionCmd createCmd = new CreateFunctionCmd(addr);
            createCmd.applyTo(currentProgram, monitor);
            fn = currentProgram.getFunctionManager().getFunctionAt(addr);
        }
        if (fn == null) {
            out.println("// STILL NOT FOUND at 0x" + Long.toHexString(va));
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

    private void dumpAt(long va) throws Exception {
        if (dumped.contains(va)) return;
        dumped.add(va);
        Address addr = toAddr(va);
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
