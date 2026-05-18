import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class DumpHUDBit14 extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\hud_bit14.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // FUN_0049fb70: confirmed writer of HUD state-flag bit 14 (0x04000).
        // Called every 5 ticks from the main flight loop; its return value is
        // OR'd into DAT_0050cfef. Goal: confirm exact carrier-approach condition.
        out.println("// === FUN_0049fb70 (HUD bit-14 / carrier-approach flag builder) ===");
        dumpAt(0x0049fb70L);

        // Also dump callers to confirm the 5-tick call pattern.
        out.println("// === Callers of FUN_0049fb70 ===");
        Set<Function> callers = getCallers(0x0049fb70L);
        out.println("// Callers found: " + callers.size());
        for (Function f : callers) {
            out.println("// CALLER: " + f.getName() + " @ " + f.getEntryPoint());
            dumpFunction(f);
        }

        out.close();
        decompiler.dispose();
        println("Output: C:\\Temp\\hud_bit14.txt");
    }

    private void dumpAt(long va) throws Exception {
        Address addr = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(va);
        Function fn = currentProgram.getFunctionManager().getFunctionAt(addr);
        if (fn == null) {
            out.println("// NOT FOUND at 0x" + Long.toHexString(va));
            return;
        }
        dumpFunction(fn);
    }

    private void dumpFunction(Function fn) throws Exception {
        DecompileResults res = decompiler.decompileFunction(fn, 60, monitor);
        if (res != null && res.getDecompiledFunction() != null) {
            out.println(res.getDecompiledFunction().getC());
        } else {
            out.println("// decompile failed: " + fn.getName());
        }
    }

    private Set<Function> getCallers(long va) {
        Set<Function> result = new LinkedHashSet<>();
        Address addr = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(va);
        ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(addr);
        while (refs.hasNext()) {
            Reference ref = refs.next();
            if (ref.getReferenceType().isCall()) {
                Function caller = currentProgram.getFunctionManager()
                        .getFunctionContaining(ref.getFromAddress());
                if (caller != null) result.add(caller);
            }
        }
        return result;
    }
}
