import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class DumpSEETransition2 extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\see_transition2.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // Context:
        //   missile+0xa6 bit 0x10000 = seeker in search mode
        //   missile+0xa6 bit 0x20000 = seeker in track mode
        //   The transition write was NOT found in:
        //     FUN_004c2eb0 (search lobe), FUN_004c31f0 (track lobe),
        //     FUN_004c2f20 (lobe dispatcher), FUN_004c4700 (guidance outer loop).
        //   FUN_004c2f20 calls FUN_004c2860 at its end — this is the prime candidate
        //   for writing the transition (cone-overlap accepted → set track bit).
        //   Also dump FUN_004c24b0 (pre-lobe call in FUN_004c4700) and
        //   FUN_004c2170 (post-success call in FUN_004c4700).
        //   Also dump FUN_004c5000 / FUN_004c5050 (lock-acquire handlers).
        //   Also dump FUN_004c52d0 (target selection, called before lobe dispatch).

        long[] targets = {
            0x004c2860L,   // seeker cone-overlap / tracking update (called at bottom of FUN_004c2f20)
            0x004c24b0L,   // pre-lobe computation in FUN_004c4700
            0x004c2170L,   // post-success call in FUN_004c4700
            0x004c5000L,   // lock-acquire handler A
            0x004c5050L,   // lock-acquire handler B
            0x004c52d0L,   // target selection (FUN_004c52d0)
        };

        for (long va : targets) {
            out.println("// === 0x" + Long.toHexString(va) + " ===");
            dumpAt(va);
        }

        // Also dump callers of FUN_004c2860 to see full call graph.
        out.println("// === Callers of FUN_004c2860 ===");
        Set<Function> callers = getCallers(0x004c2860L);
        out.println("// Callers found: " + callers.size());
        for (Function f : callers) {
            out.println("// CALLER: " + f.getName() + " @ " + f.getEntryPoint());
            dumpFunction(f);
        }

        out.close();
        decompiler.dispose();
        println("Output: C:\\Temp\\see_transition2.txt");
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
