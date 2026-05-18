import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import java.io.*;
import java.util.*;

public class DumpPROJPhysics extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\proj_physics.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // _PROJProc: missile physics callback. Reads PROJ_TYPE+0x50..0x78
        // (turn rate, g-limit, other physics params). This is the main target
        // for mapping the 55-byte gap between fuze range (+0x4F) and angle params (+0x79).
        out.println("// === _PROJProc (missile physics callback) ===");
        for (Symbol sym : currentProgram.getSymbolTable().getSymbols("_PROJProc")) {
            dumpAt(sym.getAddress().getOffset());
        }

        // FUN_004c2f20 (_PROJLock@24): missile lock function.
        // We need to find the function that WRITES missile+0xa6 transition bits
        // (0x10000 search → 0x20000 track) and sets target+0xde & 0x100000.
        // Dump all callers of _PROJLock to trace the missile service loop.
        out.println("// === _PROJLock@24 (FUN_004c2f20) ===");
        dumpAt(0x004c2f20L);

        out.println("// === Callers of FUN_004c2f20 (_PROJLock@24) ===");
        Address lockAddr = toAddr(0x004c2f20L);
        Set<Long> callerAddrs = new LinkedHashSet<>();
        for (Reference ref : currentProgram.getReferenceManager().getReferencesTo(lockAddr)) {
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

        // FUN_004c2eb0 (search lobe) and FUN_004c31f0 (track lobe) — already
        // partially analysed but dump again for the transition-write path.
        out.println("// === FUN_004c2eb0 (SEE search lobe / acquisition timer) ===");
        dumpAt(0x004c2eb0L);

        out.println("// === FUN_004c31f0 (SEE track lobe) ===");
        dumpAt(0x004c31f0L);

        // _PROJHitChance@28 and FUN_004c3960 already confirmed, but dump
        // for completeness of the PROJ_TYPE field map.
        out.println("// === _PROJHitChance@28 (FUN_004c3380) ===");
        dumpAt(0x004c3380L);

        out.println("// === FUN_004c3960 (proximity fuze check) ===");
        dumpAt(0x004c3960L);

        out.close();
        decompiler.dispose();
        println("Done. Output written to C:\\Temp\\proj_physics.txt");
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
