import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.scalar.*;
import java.io.*;
import java.util.*;

public class DumpSEETransition extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\see_transition.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // Goal: find the missile-service function that advances missile+0xa6
        // from 0x10000 (search lobe) to 0x20000 (track lobe) and sets
        // target+0xde & 0x100000. Confirmed context:
        //   FUN_004c2eb0 = search lobe (manages 40-tick acquisition timer at target+0x11a)
        //   FUN_004c31f0 = track lobe (requires target+0xde & 0x100000)
        // Neither function writes the transition bit.
        // Strategy: find all instructions with immediate 0x20000 or 0x10000;
        // find functions with +0xa6 struct-offset patterns; dump callers of
        // FUN_004c2eb0 / FUN_004c31f0 as anchors.

        // 1. Dump FUN_004c2eb0 (search lobe) and FUN_004c31f0 (track lobe)
        out.println("// === FUN_004c2eb0 (SEE search lobe) ===");
        dumpAt(0x004c2eb0L);

        out.println("// === FUN_004c31f0 (SEE track lobe) ===");
        dumpAt(0x004c31f0L);

        // 2. Dump callers of FUN_004c2eb0
        out.println("// === Callers of FUN_004c2eb0 (search lobe) ===");
        Set<Function> callers2eb0 = getCallers(0x004c2eb0L);
        out.println("// Callers found: " + callers2eb0.size());
        for (Function f : callers2eb0) {
            out.println("// CALLER: " + f.getName() + " @ " + f.getEntryPoint());
            dumpFunction(f);
        }

        // 3. Dump callers of FUN_004c31f0
        out.println("// === Callers of FUN_004c31f0 (track lobe) ===");
        Set<Function> callers31f0 = getCallers(0x004c31f0L);
        out.println("// Callers found: " + callers31f0.size());
        for (Function f : callers31f0) {
            out.println("// CALLER: " + f.getName() + " @ " + f.getEntryPoint());
            dumpFunction(f);
        }

        // 4. Search all instructions for immediate operand 0x20000 (track-mode bit)
        out.println("// === Instructions with immediate 0x20000 ===");
        findImmediate(0x20000L, "0x20000");

        // 5. Search all instructions for immediate operand 0x100000 (target+0xde bit)
        out.println("// === Instructions with immediate 0x100000 ===");
        findImmediate(0x100000L, "0x100000");

        // 6. SMS symbols matching SEE/seeker/missile/lobe patterns
        out.println("// === FA.SMS symbols matching SEE/seeker/missile/lobe ===");
        String[] patterns = {"SEE", "seeker", "Seeker", "SEEKER", "lobe", "Lobe",
                             "PROJ", "Proj", "missile", "Missile", "lock", "Lock"};
        SymbolTable symTable = currentProgram.getSymbolTable();
        SymbolIterator allSyms = symTable.getAllSymbols(true);
        while (allSyms.hasNext()) {
            Symbol sym = allSyms.next();
            String name = sym.getName();
            for (String p : patterns) {
                if (name.contains(p)) {
                    out.println("// SYM: " + name + " @ " + sym.getAddress());
                    break;
                }
            }
        }

        out.close();
        decompiler.dispose();
        println("Output: C:\\Temp\\see_transition.txt");
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

    private void findImmediate(long value, String label) throws Exception {
        Set<Function> seen = new LinkedHashSet<>();
        InstructionIterator iter = currentProgram.getListing().getInstructions(true);
        while (iter.hasNext()) {
            Instruction instr = iter.next();
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
        out.println("// Total functions with " + label + ": " + seen.size());
    }
}
