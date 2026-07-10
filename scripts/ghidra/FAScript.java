import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.*;
import ghidra.program.model.mem.*;
import java.io.*;
import java.util.*;

public abstract class FAScript extends GhidraScript {

    protected DecompInterface decompiler;
    protected PrintWriter out;
    protected Set<Long> dumped;
    protected String outputPath;

    @Override
    public void run() throws Exception {}

    // -----------------------------------------------------------------------
    // Output lifecycle
    // -----------------------------------------------------------------------

    protected void openOutput(String name) throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        dumped = new LinkedHashSet<>();
        String projectDir = System.getenv("FA_PROJECT");
        if (projectDir == null || projectDir.isEmpty())
            projectDir = System.getProperty("java.io.tmpdir");
        File outDir = new File(projectDir, "output");
        outDir.mkdirs();
        File outFile = new File(outDir, name + ".txt");
        outputPath = outFile.getAbsolutePath();
        out = new PrintWriter(new FileWriter(outFile));
    }

    /** Append mode: used by overlay-DLL scripts that run once per DLL in a batch. */
    protected void openOutputAppend(String name) throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        dumped = new LinkedHashSet<>();
        String projectDir = System.getenv("FA_PROJECT");
        if (projectDir == null || projectDir.isEmpty())
            projectDir = System.getProperty("java.io.tmpdir");
        File outDir = new File(projectDir, "output");
        outDir.mkdirs();
        File outFile = new File(outDir, name + ".txt");
        outputPath = outFile.getAbsolutePath();
        out = new PrintWriter(new FileWriter(outFile, true));   // append
        out.println("// ============================================================");
        out.println("// " + name + " -- " + currentProgram.getName());
        out.println("// Image base: 0x"
                + Long.toHexString(currentProgram.getImageBase().getOffset()));
        out.println("// ============================================================");
        out.println();
    }

    protected void closeOutput() {
        if (out != null) {
            out.close(); out = null;
            println("Output: " + outputPath);
        }
        if (decompiler != null) { decompiler.dispose(); decompiler = null; }
    }

    protected void header(String title) {
        out.println("\n// === " + title + " ===");
    }

    // -----------------------------------------------------------------------
    // Function dump helpers
    // -----------------------------------------------------------------------

    protected void dumpAt(long va) throws Exception {
        if (dumped.contains(va)) return;
        dumped.add(va);
        Address addr = toAddr(va);
        FunctionManager fm = currentProgram.getFunctionManager();
        Function fn = fm.getFunctionAt(addr);
        if (fn == null) fn = fm.getFunctionContaining(addr);
        if (fn == null) {
            out.println("// NOT FOUND at 0x" + Long.toHexString(va));
            return;
        }
        dumpFunction(fn);
    }

    protected void dumpAtForced(long va) throws Exception {
        if (dumped.contains(va)) return;
        dumped.add(va);
        Address addr = toAddr(va);
        FunctionManager fm = currentProgram.getFunctionManager();
        Function fn = fm.getFunctionAt(addr);
        if (fn == null) {
            DisassembleCommand disCmd = new DisassembleCommand(addr, null, true);
            disCmd.applyTo(currentProgram, monitor);
            CreateFunctionCmd createCmd = new CreateFunctionCmd(addr);
            createCmd.applyTo(currentProgram, monitor);
            fn = fm.getFunctionAt(addr);
        }
        if (fn == null) {
            out.println("// STILL NOT FOUND at 0x" + Long.toHexString(va));
            return;
        }
        dumpFunction(fn);
    }

    protected void dumpFunction(Function fn) throws Exception {
        out.println("// --- " + fn.getName() + " @ " + fn.getEntryPoint() + " ---");
        DecompileResults res = decompiler.decompileFunction(fn, 120, monitor);
        if (res != null && res.getDecompiledFunction() != null) {
            out.println(res.getDecompiledFunction().getC());
        } else {
            out.println("// decompile failed: " + fn.getName());
        }
        out.println();
    }

    protected void dumpCallers(long va) throws Exception {
        Address addr = toAddr(va);
        FunctionManager fm = currentProgram.getFunctionManager();
        Function target = fm.getFunctionAt(addr);
        if (target == null) target = fm.getFunctionContaining(addr);
        String name = target != null ? target.getName() : "0x" + Long.toHexString(va);
        out.println("// Callers of " + name + " (0x" + Long.toHexString(va) + "):");
        for (long callerVa : findCallers(va)) {
            Function fn = fm.getFunctionAt(toAddr(callerVa));
            if (fn != null) out.println("//   " + fn.getName() + " @ 0x" + Long.toHexString(callerVa));
        }
        for (long callerVa : findCallers(va)) dumpAt(callerVa);
    }

    /** Dump all functions that hold data references to the given address. */
    protected void dumpXrefsToData(long va) throws Exception {
        dumpXrefsToData(va, false);
    }

    /** Dump functions that hold data references to the given address.
     *  @param writesOnly  when true only WRITE references are followed */
    protected void dumpXrefsToData(long va, boolean writesOnly) throws Exception {
        Address addr = toAddr(va);
        for (Reference ref : currentProgram.getReferenceManager().getReferencesTo(addr)) {
            if (writesOnly && !ref.getReferenceType().isWrite()) continue;
            Function fn = currentProgram.getFunctionManager()
                    .getFunctionContaining(ref.getFromAddress());
            if (fn != null) dumpAt(fn.getEntryPoint().getOffset());
        }
    }

    protected Set<Long> findCallers(long va) throws Exception {
        Address addr = toAddr(va);
        Set<Long> callers = new LinkedHashSet<>();
        for (Reference ref : currentProgram.getReferenceManager().getReferencesTo(addr)) {
            if (ref.getReferenceType().isCall()) {
                Function fn = currentProgram.getFunctionManager()
                        .getFunctionContaining(ref.getFromAddress());
                if (fn != null) callers.add(fn.getEntryPoint().getOffset());
            }
        }
        return callers;
    }

    protected Set<Long> findFunctionsWithMask(long start, long end, long mask) throws Exception {
        AddressSpace space = currentProgram.getAddressFactory().getDefaultAddressSpace();
        Listing listing = currentProgram.getListing();
        Address endAddr = space.getAddress(end);
        Set<Long> seen = new LinkedHashSet<>();
        InstructionIterator instrs = listing.getInstructions(space.getAddress(start), true);
        while (instrs.hasNext()) {
            Instruction instr = instrs.next();
            if (instr.getAddress().compareTo(endAddr) > 0) break;
            String mnem = instr.getMnemonicString().toLowerCase();
            if (!mnem.equals("and") && !mnem.equals("test") && !mnem.equals("or")) continue;
            for (int i = 0; i < instr.getNumOperands(); i++) {
                for (Object obj : instr.getOpObjects(i)) {
                    if (obj instanceof Scalar) {
                        long val = ((Scalar) obj).getUnsignedValue();
                        if (val == mask) {
                            Function fn = currentProgram.getFunctionManager()
                                    .getFunctionContaining(instr.getAddress());
                            if (fn != null) seen.add(fn.getEntryPoint().getOffset());
                        }
                    }
                }
            }
        }
        return seen;
    }

    protected Set<Long> findFunctionsReadingOffsets(long start, long end, int minOff, int maxOff) throws Exception {
        AddressSpace space = currentProgram.getAddressFactory().getDefaultAddressSpace();
        Listing listing = currentProgram.getListing();
        Address endAddr = space.getAddress(end);
        Set<Long> seen = new LinkedHashSet<>();
        InstructionIterator instrs = listing.getInstructions(space.getAddress(start), true);
        while (instrs.hasNext()) {
            Instruction instr = instrs.next();
            if (instr.getAddress().compareTo(endAddr) > 0) break;
            for (int i = 0; i < instr.getNumOperands(); i++) {
                for (Object obj : instr.getOpObjects(i)) {
                    if (obj instanceof Scalar) {
                        long val = ((Scalar) obj).getUnsignedValue();
                        if (val >= minOff && val <= maxOff) {
                            Function fn = currentProgram.getFunctionManager()
                                    .getFunctionContaining(instr.getAddress());
                            if (fn != null && seen.add(fn.getEntryPoint().getOffset())) {
                                out.println("// offset 0x" + Long.toHexString(val)
                                        + " in " + fn.getName() + " @ " + fn.getEntryPoint());
                            }
                        }
                    }
                }
            }
        }
        return seen;
    }

    protected void searchBitTestsInRange(long start, long end, long mask) throws Exception {
        for (long va : findFunctionsWithMask(start, end, mask)) {
            Function fn = currentProgram.getFunctionManager().getFunctionAt(toAddr(va));
            if (fn == null) fn = currentProgram.getFunctionManager().getFunctionContaining(toAddr(va));
            String name = fn != null ? fn.getName() : "?";
            out.println("// mask 0x" + Long.toHexString(mask) + " in " + name + " @ 0x" + Long.toHexString(va));
            dumpAt(va);
        }
    }

    /** Search for ASCII strings anywhere in the program's address space. */
    protected void searchStrings(String[] patterns) throws Exception {
        Address searchStart = currentProgram.getMinAddress();
        Address searchEnd   = currentProgram.getMaxAddress();
        ReferenceManager rm = currentProgram.getReferenceManager();
        FunctionManager fm  = currentProgram.getFunctionManager();
        for (String kw : patterns) {
            byte[] kwBytes = kw.getBytes("US-ASCII");
            Address found = currentProgram.getMemory().findBytes(
                    searchStart, searchEnd, kwBytes, null, true, monitor);
            while (found != null) {
                out.println("// string '" + kw + "' at " + found);
                for (Reference ref : rm.getReferencesTo(found)) {
                    Function f = fm.getFunctionContaining(ref.getFromAddress());
                    if (f != null && dumped.add(f.getEntryPoint().getOffset())) {
                        out.println("//   ref from " + ref.getFromAddress()
                                + " in " + f.getName() + " @ " + f.getEntryPoint());
                        dumpAt(f.getEntryPoint().getOffset());
                    }
                }
                found = currentProgram.getMemory().findBytes(
                        found.add(1), searchEnd, kwBytes, null, true, monitor);
            }
        }
    }

    protected void dumpRange(long start, long end) throws Exception {
        for (Function fn : currentProgram.getFunctionManager().getFunctions(true)) {
            long ep = fn.getEntryPoint().getOffset();
            if (ep >= start && ep <= end && !dumped.contains(ep)) dumpAt(ep);
        }
    }

    protected void dumpSymbolsMatching(String... keywords) throws Exception {
        for (Symbol sym : currentProgram.getSymbolTable().getAllSymbols(false)) {
            String name = sym.getName().toLowerCase();
            for (String kw : keywords) {
                if (name.contains(kw)) {
                    out.println("// SYM: " + sym.getName() + " @ " + sym.getAddress());
                    Function fn = currentProgram.getFunctionManager().getFunctionAt(sym.getAddress());
                    if (fn == null) fn = currentProgram.getFunctionManager().getFunctionContaining(sym.getAddress());
                    if (fn != null) dumpAt(fn.getEntryPoint().getOffset());
                    break;
                }
            }
        }
    }
}
