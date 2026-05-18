import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.scalar.*;
import java.io.*;
import java.util.*;

public class DumpSEETransition3 extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\see_transition3.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // Still hunting for the write of missile+0xa6 bits 0x10000/0x20000.
        // FUN_004c2170 ends by calling FUN_004c26f0 — that's the prime candidate.
        // FUN_004c0a90 is the per-step guidance update called from FUN_004c2170.
        // FUN_004c3eb0 is called from FUN_004c2f20 when missile+0xa6 & 0x10 is set.
        // Also dump FUN_004c4100 (the target re-evaluation sub in FUN_004c4700).

        long[] targets = {
            0x004c26f0L,  // final-step call in FUN_004c2170 (guidance wrapper)
            0x004c0a90L,  // per-step guidance update in FUN_004c2170
            0x004c3eb0L,  // called from FUN_004c2f20 when missile+0xa6 & 0x10 is set
            0x004c4100L,  // target re-eval sub called from FUN_004c4700
            0x004c58a0L,  // called from FUN_004c2170 when missile+0xa6 & 0x200000 != 0
        };

        for (long va : targets) {
            out.println("// === 0x" + Long.toHexString(va) + " ===");
            dumpAt(va);
        }

        // Also find all writes to immediate 0x10000 — these should include
        // the missile initialisation write (which sets the search-lobe bit).
        // Look for writes specifically inside the 004c0000–004c7000 range.
        out.println("// === Instructions with immediate 0x10000 in 004c0000-004c7000 ===");
        findImmediateInRange(0x10000L, 0x004c0000L, 0x004c7000L, "0x10000");

        out.close();
        decompiler.dispose();
        println("Output: C:\\Temp\\see_transition3.txt");
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

    private void findImmediateInRange(long value, long startVA, long endVA, String label) throws Exception {
        AddressSpace space = currentProgram.getAddressFactory().getDefaultAddressSpace();
        Address start = space.getAddress(startVA);
        Address end = space.getAddress(endVA);
        Set<Function> seen = new LinkedHashSet<>();

        InstructionIterator iter = currentProgram.getListing().getInstructions(start, true);
        while (iter.hasNext()) {
            Instruction instr = iter.next();
            if (instr.getAddress().compareTo(end) > 0) break;
            for (int i = 0; i < instr.getNumOperands(); i++) {
                Object[] objs = instr.getOpObjects(i);
                for (Object obj : objs) {
                    if (obj instanceof Scalar) {
                        Scalar s = (Scalar) obj;
                        if (s.getUnsignedValue() == value) {
                            Function fn = currentProgram.getFunctionManager()
                                    .getFunctionContaining(instr.getAddress());
                            if (fn != null && !seen.contains(fn)) {
                                seen.add(fn);
                                out.println("// MATCH " + label + " in: "
                                        + fn.getName() + " @ " + fn.getEntryPoint()
                                        + " (instr @ " + instr.getAddress() + ")");
                                dumpFunction(fn);
                            }
                        }
                    }
                }
            }
        }
        out.println("// Total functions with " + label + " in range: " + seen.size());
    }
}
