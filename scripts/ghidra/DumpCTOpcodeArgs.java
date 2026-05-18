import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import java.io.*;
import java.util.*;

public class DumpCTOpcodeArgs extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;
    private Set<Long> dumped = new LinkedHashSet<>();

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\ct_opcode_args.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // Goal 1: FUN_00466a80 — the BI bytecode opcode dispatcher.
        // Called by CTExecProgram each iteration; reads one opcode from DAT_00546bea
        // and dispatches to the matching CTDo_* / CTEval_* handler.
        // Was NOT in the 0x466800-0x466a00 range — must be dumped explicitly.
        out.println("// === FUN_00466a80 (BI opcode dispatcher) ===");
        dumpAtForced(0x00466a80L);

        // Goal 2: CTDo_ argument-reader helpers.
        // All are in 0x465c00-0x465f00; called by CTDo_move/jink/turn to pull
        // arguments from the bytecode stream at DAT_00546bea.
        out.println("\n// === Arg reader cluster 0x465c00-0x465f00 ===");
        for (Function fn : currentProgram.getFunctionManager().getFunctions(true)) {
            long ep = fn.getEntryPoint().getOffset();
            if (ep >= 0x00465c00L && ep <= 0x00465f00L && !dumped.contains(ep)) {
                dumpAt(ep);
            }
        }
        // Force-create any unlabelled ones in that range.
        long[] forcedArgReaders = {
            0x00465c90L, 0x00465d40L, 0x00465da0L, 0x00465de0L, 0x00465e00L
        };
        for (long va : forcedArgReaders) dumpAtForced(va);

        // FUN_00465ad0 — also used in CTDo_jink as an additional arg reader.
        out.println("\n// === FUN_00465ad0 (additional arg reader / CTDo_jink) ===");
        dumpAtForced(0x00465ad0L);

        // Goal 3: _MVRJink@40 was NOT FOUND at 0x4ac9e0 via symbol lookup;
        // force-create it via disassembly.
        out.println("\n// === _MVRJink@40 (forced at 0x4ac9e0) ===");
        dumpAtForced(0x004ac9e0L);

        // _MVRMove@? (0x4ac510) — called by CTDo_move; will confirm arg order.
        out.println("\n// === FUN_004ac510 (_MVRMove) ===");
        dumpAtForced(0x004ac510L);

        // Goal 4: PROJ_TYPE+0x50-0x6E gap — search functions that access
        // missile entity at offsets 0xF6-0x114 (= PROJ_TYPE+0x50-0x6E since
        // PROJ_TYPE starts at entity+0xa6).
        // Known candidates: _PROJLock@24 (0x4c2f20) and cluster 0x4c2800-0x4c3100.
        out.println("\n// === _PROJLock@24 (0x4c2f20) ===");
        dumpAt(0x004c2f20L);

        out.println("\n// === Functions 0x4c2800-0x4c3100 (PROJ lock/hit cluster) ===");
        for (Function fn : currentProgram.getFunctionManager().getFunctions(true)) {
            long ep = fn.getEntryPoint().getOffset();
            if (ep >= 0x004c2800L && ep <= 0x004c3100L && !dumped.contains(ep)) {
                dumpAt(ep);
            }
        }

        out.close();
        decompiler.dispose();
        println("Output: C:\\Temp\\ct_opcode_args.txt");
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
