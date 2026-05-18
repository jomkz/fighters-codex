import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import java.io.*;
import java.util.*;

public class DumpT2Loader extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;
    private Set<Long> dumpedAddrs = new LinkedHashSet<>();

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\t2_loader.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // T2 terrain file loader search.
        // Prior attempts: symbol-name matching found nothing — FA.SMS has no T2-related symbols.
        // New strategies:
        //   1. Search for "BIT2" magic bytes in PE data, find functions that reference it.
        //   2. Search for "textFormat" / "map" keywords to find the MM text parser.
        //   3. Scan for the T2 sub-header size constant (0x80 + 21 = 0x95) and dim constants.
        //   4. Decompile callers of the MM load chain from known entry points.

        AddressFactory af = currentProgram.getAddressFactory();
        AddressSpace ds = af.getDefaultAddressSpace();
        ReferenceManager rm = currentProgram.getReferenceManager();
        FunctionManager fm = currentProgram.getFunctionManager();

        // -------------------------------------------------------
        // PART 1: Search for "BIT2" magic in PE data/code
        // -------------------------------------------------------
        out.println("// === PART 1: 'BIT2' magic (0x42 0x49 0x54 0x32) in PE ===");
        byte[] bit2 = { 0x42, 0x49, 0x54, 0x32 };
        Address searchStart = ds.getAddress(0x00400000L);
        Address searchEnd   = ds.getAddress(0x00600000L);
        Address found = currentProgram.getMemory().findBytes(
            searchStart, searchEnd, bit2, null, true, monitor);
        int bit2Count = 0;
        while (found != null) {
            bit2Count++;
            out.println("// 'BIT2' at " + found);
            for (Reference ref : rm.getReferencesTo(found)) {
                Function f = fm.getFunctionContaining(ref.getFromAddress());
                if (f != null) {
                    out.println("//   ref from " + ref.getFromAddress() + " in " + f.getName()
                            + " @ " + f.getEntryPoint() + " type=" + ref.getReferenceType());
                    dumpAt(f.getEntryPoint().getOffset());
                }
            }
            found = currentProgram.getMemory().findBytes(
                found.add(1), searchEnd, bit2, null, true, monitor);
        }
        out.println("// BIT2 occurrences: " + bit2Count);
        out.println();

        // -------------------------------------------------------
        // PART 2: Search for "textFormat" and "map" strings
        //         (MM file keywords — the parser reads these)
        // -------------------------------------------------------
        out.println("// === PART 2: MM parser keyword strings ===");
        String[] keywords = { "textFormat", "map", ".T2", "T2", "tmap", "tdic" };
        for (String kw : keywords) {
            byte[] kwBytes = kw.getBytes("US-ASCII");
            Address kFound = currentProgram.getMemory().findBytes(
                searchStart, searchEnd, kwBytes, null, true, monitor);
            while (kFound != null) {
                out.println("// '" + kw + "' at " + kFound);
                for (Reference ref : rm.getReferencesTo(kFound)) {
                    Function f = fm.getFunctionContaining(ref.getFromAddress());
                    if (f != null) {
                        out.println("//   ref from " + ref.getFromAddress() + " in " + f.getName()
                                + " @ " + f.getEntryPoint());
                        dumpAt(f.getEntryPoint().getOffset());
                    }
                }
                kFound = currentProgram.getMemory().findBytes(
                    kFound.add(1), searchEnd, kwBytes, null, true, monitor);
            }
        }
        out.println();

        // -------------------------------------------------------
        // PART 3: FA.SMS symbols matching terrain/tile/T2
        // -------------------------------------------------------
        out.println("// === PART 3: FA.SMS symbols matching T2/terrain/tile ===");
        for (Symbol sym : currentProgram.getSymbolTable().getAllSymbols(false)) {
            String name = sym.getName().toLowerCase();
            if (name.contains("terrain") || name.contains("tile") || name.contains("t2")
                    || name.contains("surface") || name.contains("tmap") || name.contains("tdic")
                    || name.contains("mapload") || name.contains("mapinit")) {
                out.println("// SYM: " + sym.getName() + " @ " + sym.getAddress());
                Address symAddr = sym.getAddress();
                for (Reference ref : rm.getReferencesTo(symAddr)) {
                    Function f = fm.getFunctionContaining(ref.getFromAddress());
                    if (f != null && dumpedAddrs.add(f.getEntryPoint().getOffset())) {
                        out.println("//   ref from " + ref.getFromAddress() + " in " + f.getName());
                        dumpAt(f.getEntryPoint().getOffset());
                    }
                }
                Function sym_fn = fm.getFunctionAt(symAddr);
                if (sym_fn == null) sym_fn = fm.getFunctionContaining(symAddr);
                if (sym_fn != null) dumpAt(sym_fn.getEntryPoint().getOffset());
            }
        }
        out.println();

        // -------------------------------------------------------
        // PART 4: Scan all instructions for T2 sub-header constants
        //         0x95 = absolute offset of first tile (0x80 + 21)
        //         0x80 = payload start
        //         0xC3 = 195 (bytes-per-tile)
        // -------------------------------------------------------
        out.println("// === PART 4: Functions using T2 sub-header constants (0x95, 0x80, 195) ===");
        long[] t2Constants = { 0x95L, 0x80L, 195L, 21L };
        for (long c : t2Constants) {
            Set<Long> hits = new LinkedHashSet<>();
            Listing listing = currentProgram.getListing();
            InstructionIterator insns = listing.getInstructions(ds.getAddress(0x00400000L), true);
            while (insns.hasNext()) {
                Instruction insn = insns.next();
                if (insn.getAddress().getOffset() > 0x00600000L) break;
                for (int i = 0; i < insn.getNumOperands(); i++) {
                    for (Object o : insn.getOpObjects(i)) {
                        if (o instanceof ghidra.program.model.scalar.Scalar) {
                            long val = ((ghidra.program.model.scalar.Scalar)o).getUnsignedValue();
                            if (val == c) {
                                Function f = fm.getFunctionContaining(insn.getAddress());
                                if (f != null && hits.add(f.getEntryPoint().getOffset())) {
                                    out.println("// const 0x" + Long.toHexString(c)
                                            + " in " + f.getName() + " @ " + f.getEntryPoint());
                                }
                            }
                        }
                    }
                }
            }
        }
        out.println();

        // -------------------------------------------------------
        // PART 5: Force-decompile known T2-adjacent addresses
        // -------------------------------------------------------
        out.println("// === PART 5: Known adjacent functions ===");
        long[] forcedAddrs = {
            0x00447aa5L, // @G_Tile@32 (terrain tile renderer — known, for reference)
            0x004a6cc0L, // sub-resource DLL loader called by @G_Tile@32
            0x00422380L, // ?MAPWorldToScreen (confirmed)
            // Addresses in the MM/theater load range 0x422000-0x427000
            0x00422000L, 0x00423000L, 0x00424000L, 0x00425000L,
            0x00426000L, 0x00427000L,
        };
        for (long addr : forcedAddrs) {
            Function f = fm.getFunctionAt(ds.getAddress(addr));
            if (f == null) f = fm.getFunctionContaining(ds.getAddress(addr));
            if (f != null) dumpAt(f.getEntryPoint().getOffset());
        }

        out.close();
        decompiler.dispose();
        println("Done. Output written to C:\\Temp\\t2_loader.txt");
    }

    private void dumpAt(long address) throws Exception {
        if (dumpedAddrs.contains(address)) return;
        dumpedAddrs.add(address);
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
