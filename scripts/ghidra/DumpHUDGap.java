import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import java.io.*;
import java.util.*;

public class DumpHUDGap extends GhidraScript {

    @Override
    public void run() throws Exception {
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\hud_gap.txt");
        outFile.getParentFile().mkdirs();
        PrintWriter out = new PrintWriter(new FileWriter(outFile));

        // HUD struct is bulk-copied to DAT_00521360.
        // Unknown gap: struct offsets 0x238-0x263 (globals 0x521598-0x5215c3).
        // Probe every 2 bytes across the gap for cross-references to discover reader functions.
        out.println("// === Cross-references to HUD struct gap (0x521598 - 0x5215c3) ===");

        Set<Long> callers = new LinkedHashSet<>();
        for (long probe = 0x521598L; probe <= 0x5215c3L; probe += 2) {
            Address probeAddr = currentProgram.getAddressFactory()
                .getDefaultAddressSpace().getAddress(probe);
            Iterator<ghidra.program.model.symbol.Reference> refs =
                currentProgram.getReferenceManager().getReferencesTo(probeAddr);
            boolean any = false;
            while (refs.hasNext()) {
                ghidra.program.model.symbol.Reference ref = refs.next();
                out.println("//   " + ref.getReferenceType()
                    + " from " + ref.getFromAddress()
                    + " → 0x" + Long.toHexString(probe));
                Function f = currentProgram.getFunctionManager()
                    .getFunctionContaining(ref.getFromAddress());
                if (f != null) callers.add(f.getEntryPoint().getOffset());
                any = true;
            }
            if (!any) {
                // also probe odd offsets in case s8/u8 fields
                Address probeOdd = currentProgram.getAddressFactory()
                    .getDefaultAddressSpace().getAddress(probe + 1);
                Iterator<ghidra.program.model.symbol.Reference> refsOdd =
                    currentProgram.getReferenceManager().getReferencesTo(probeOdd);
                while (refsOdd.hasNext()) {
                    ghidra.program.model.symbol.Reference ref = refsOdd.next();
                    out.println("//   " + ref.getReferenceType()
                        + " from " + ref.getFromAddress()
                        + " → 0x" + Long.toHexString(probe + 1));
                    Function f = currentProgram.getFunctionManager()
                        .getFunctionContaining(ref.getFromAddress());
                    if (f != null) callers.add(f.getEntryPoint().getOffset());
                }
            }
        }
        out.println();

        // Decompile each unique reader function
        for (long addr : callers) {
            dumpAt(addr, decompiler, out);
        }

        // Also decompile the HUD init + known draw functions for anchor-point context
        out.println("// === HUD init (FUN_00406040) for anchor-point / 0x10 0x10 context ===");
        dumpAt(0x00406040L, decompiler, out);

        out.println("// === ?HUDDrawHVel (FUN_00408e20) ===");
        dumpAt(0x00408E20L, decompiler, out);

        out.close();
        decompiler.dispose();
        println("Done. Output written to C:\\Temp\\hud_gap.txt");
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
