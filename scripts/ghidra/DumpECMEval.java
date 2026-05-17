import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import java.io.*;
import java.util.*;

public class DumpECMEval extends GhidraScript {

    @Override
    public void run() throws Exception {
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\ecm_eval.txt");
        outFile.getParentFile().mkdirs();
        PrintWriter out = new PrintWriter(new FileWriter(outFile));

        // FUN_00452770 is the actual ECM evaluator.
        // Called as: FUN_00452770(uVar1, (int*)0x0, (int*)0x0) from @HARDFindECMForObj@8
        // Previous session confirmed: bytes +0x0A and +0x0E of the ECM struct are
        // not yet decoded. The reader for these is expected to be inside FUN_00452770
        // or its immediate callees.
        // Also decompile @HARDFindECMForObj@8 (FUN_00452770's caller) for context.

        out.println("// === FUN_00452770 (ECM evaluator) ===");
        dumpAt(0x00452770L, decompiler, out);

        // Find callers of FUN_00452770 to get full calling context
        out.println("// === Callers of FUN_00452770 ===");
        Address evalAddr = currentProgram.getAddressFactory()
            .getDefaultAddressSpace().getAddress(0x00452770L);
        Set<Long> callers = new LinkedHashSet<>();
        currentProgram.getReferenceManager().getReferencesTo(evalAddr).forEach(ref -> {
            if (ref.getReferenceType().isCall()) {
                Function f = currentProgram.getFunctionManager()
                    .getFunctionContaining(ref.getFromAddress());
                if (f != null) {
                    out.println("//   from " + f.getName() + " @ " + f.getEntryPoint());
                    callers.add(f.getEntryPoint().getOffset());
                }
            }
        });
        out.println();
        for (long addr : callers) {
            dumpAt(addr, decompiler, out);
        }

        // Also decompile the SEE missile-service candidates:
        // Find functions that WRITE to missile+0xa6 by searching for writes
        // to globals near the missile struct. The missile struct base is at
        // ~DAT_0050cf9c (player missile pointer). Look for writes to addresses
        // that contain 0xa6 offset patterns.
        // Strategy: search for cross-references (WRITE) to known missile-struct
        // globals in the 0xa6 area. The flag written is 0x20000 (track mode).
        //
        // Also: JT warhead/agility — decompile @WEAPONGetHitProb or similar
        // Likely near the fuze Pk function FUN_004c3960 and FUN_004c1870.

        out.println("// === FUN_004c1870 (?PROJDamageProc — may read JT warhead flags) ===");
        dumpAt(0x004c1870L, decompiler, out);

        // FUN_0040f970 (_DAMAGEDoHit@12) — may read JT warhead type for blast radius
        out.println("// === FUN_0040f970 (_DAMAGEDoHit@12) ===");
        dumpAt(0x0040f970L, decompiler, out);

        // Find write references to the missile struct +0xa6 area.
        // DAT_0050ce91 is likely a missile/entity base; +0xa6 ~= 0x0050cf37
        // But more reliably: search for writes referencing the known missile globals.
        // The missile search flag 0x10000 and track flag 0x20000 written to missile+0xa6
        // — find who writes those values.
        out.println("// === Functions writing missile-mode flags (search for 0x20000 write refs) ===");
        // Try to find callers that write to offset 0xa6 of the missile struct.
        // We'll look for refs to FUN_004c0960 (_PROJLockUpdate), which was identified
        // as counting missiles — it may not set the flag but may call who does.
        out.println("// === FUN_004c0960 (_PROJLockUpdate) ===");
        dumpAt(0x004c0960L, decompiler, out);

        // Also try FUN_004c2f20 (_PROJLock@24) full body
        out.println("// === FUN_004c2f20 (_PROJLock@24) ===");
        dumpAt(0x004c2f20L, decompiler, out);

        out.close();
        decompiler.dispose();
        println("Done. Output written to C:\\Temp\\ecm_eval.txt");
    }

    private void dumpAt(long address, DecompInterface decompiler, PrintWriter out) throws Exception {
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
