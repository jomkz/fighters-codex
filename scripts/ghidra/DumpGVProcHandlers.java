import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import java.io.*;
import java.util.*;

public class DumpGVProcHandlers extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;
    private Set<Long> dumped = new LinkedHashSet<>();

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\gvproc_handlers.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // _GVProc @ 0x00473DB0 dispatches:
        //   param_1 == 3 -> FUN_00473f50 (vehicle init handler)
        //   param_1 == 5 -> LAB_004736f0 (vehicle draw handler, inside another function)
        //   otherwise    -> FUN_00473be0(param_1) (general AI handler)
        //
        // Goal: confirm NT hardpoint bit 1 ($2) meaning via the fire-control path.
        // Also: dump CTDo_ jump table if it exists.

        out.println("// === FUN_00473f50 (GVProc param=3 handler) ===");
        dumpAt(0x00473f50L);

        out.println("\n// === FUN_00473be0 (GVProc general handler) ===");
        dumpAt(0x00473be0L);

        // LAB_004736f0 is inside a function — find it
        out.println("\n// === Function containing LAB_004736f0 ===");
        Address labAddr = toAddr(0x004736f0L);
        Function fnLab = currentProgram.getFunctionManager().getFunctionContaining(labAddr);
        if (fnLab != null) {
            out.println("// LAB_004736f0 is in: " + fnLab.getName() + " @ " + fnLab.getEntryPoint());
            dumpAt(fnLab.getEntryPoint().getOffset());
        } else {
            out.println("// LAB_004736f0 not in any recognized function — forcing creation");
            dumpAtForced(0x004736f0L);
        }

        // Also look for the vehicle proc table — _GVProc returns function ptrs,
        // so there should be a dispatch table or direct call chain.
        // Dump the function that contains the ship fire-control weapon selection.
        // Search for FA.SMS symbols matching ship/vessel/naval/vehicle AI.
        out.println("\n// === FA.SMS symbols matching ship/vehicle/naval/GV ===");
        for (Symbol sym : currentProgram.getSymbolTable().getAllSymbols(false)) {
            String name = sym.getName().toLowerCase();
            if (name.contains("_gv") || name.contains("ship") || name.contains("vessel")
                    || name.contains("_naval") || name.contains("naval_")
                    || name.contains("hardfire") || name.contains("hard_fire")) {
                out.println("// SYM: " + sym.getName() + " @ " + sym.getAddress());
            }
        }

        // Dump callers of _HARDFindProj@16 (0x00452FF0) — fire-control loop.
        out.println("\n// === Callers of _HARDFindProj@16 (0x00452FF0) ===");
        Address hardFindAddr = toAddr(0x00452FF0L);
        Set<Long> callersSeen = new LinkedHashSet<>();
        for (Reference ref : currentProgram.getReferenceManager().getReferencesTo(hardFindAddr)) {
            if (ref.getReferenceType().isCall()) {
                Function fn = currentProgram.getFunctionManager()
                        .getFunctionContaining(ref.getFromAddress());
                if (fn != null && callersSeen.add(fn.getEntryPoint().getOffset())) {
                    out.println("// CALLER: " + fn.getName() + " @ " + fn.getEntryPoint());
                }
            }
        }
        for (long va : callersSeen) dumpAt(va);

        // For the AI CTDo_ table: _CTDo_move at 0x465CC0 is NOT a function entry per Ghidra.
        // These are likely entries in an opcode dispatch table (jump table).
        // Force function creation at the CTDo addresses and dump them.
        long[] ctdoAddrs = {
            0x00465CC0L,  // _CTDo_move
            0x00465E20L,  // _CTDo_movetoalt
            0x004663F0L,  // _CTDo_jink
        };
        out.println("\n// === Force-create CTDo_ functions ===");
        for (long va : ctdoAddrs) {
            out.println("// Forcing at 0x" + Long.toHexString(va));
            dumpAtForced(va);
        }

        // Also try to find the dispatch table by looking for function references
        // to the ctdo addresses in the data/code sections.
        out.println("\n// === References to 0x00465CC0 (_CTDo_move) ===");
        Address moveAddr = toAddr(0x00465CC0L);
        for (Reference ref : currentProgram.getReferenceManager().getReferencesTo(moveAddr)) {
            out.println("// ref at " + ref.getFromAddress() + " type=" + ref.getReferenceType());
            Function fn = currentProgram.getFunctionManager().getFunctionContaining(ref.getFromAddress());
            if (fn != null) {
                out.println("//   in " + fn.getName() + " @ " + fn.getEntryPoint());
                dumpAt(fn.getEntryPoint().getOffset());
            }
        }

        out.close();
        decompiler.dispose();
        println("Output: C:\\Temp\\gvproc_handlers.txt");
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
