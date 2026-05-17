import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import java.io.*;
import java.util.*;

public class DumpMUSCallbacks extends GhidraScript {

    @Override
    public void run() throws Exception {
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\mus_callbacks.txt");
        outFile.getParentFile().mkdirs();
        PrintWriter out = new PrintWriter(new FileWriter(outFile));

        // MUS CODE sections are passed directly to _AIL_init_sequence.
        // FA/FB/FC are internal AIL XMIDI extensions — not interpretable from FA.EXE.
        // FE/FD branch conditions may use FA.EXE callbacks registered with AIL.
        // Goal: find _AIL_init_sequence import, locate all callers, decompile them
        // to see what callback pointers they pass; then decompile those callbacks.

        // Step 1: find _AIL_init_sequence by symbol name
        out.println("// === Searching for _AIL_init_sequence ===");
        SymbolTable symTable = currentProgram.getSymbolTable();
        List<Long> ailInitCallers = new ArrayList<>();

        SymbolIterator syms = symTable.getSymbols("_AIL_init_sequence");
        if (!syms.hasNext()) {
            // try without leading underscore or with different casing
            syms = symTable.getSymbols("AIL_init_sequence");
        }
        Address ailInitAddr = null;
        while (syms.hasNext()) {
            Symbol sym = syms.next();
            out.println("// Found: " + sym.getName() + " @ " + sym.getAddress()
                + " [" + sym.getSymbolType() + "]");
            ailInitAddr = sym.getAddress();
        }
        if (ailInitAddr == null) {
            out.println("// _AIL_init_sequence not found by name — searching imports...");
            // Walk external symbols / thunks
            SymbolIterator allSyms = symTable.getAllSymbols(false);
            while (allSyms.hasNext()) {
                Symbol s = allSyms.next();
                if (s.getName().toLowerCase().contains("init_sequence")) {
                    out.println("// Candidate: " + s.getName() + " @ " + s.getAddress());
                    ailInitAddr = s.getAddress();
                }
            }
        }
        out.println();

        // Step 2: find callers of _AIL_init_sequence
        if (ailInitAddr != null) {
            out.println("// === Callers of _AIL_init_sequence @ " + ailInitAddr + " ===");
            Set<Long> callerSet = new LinkedHashSet<>();
            currentProgram.getReferenceManager().getReferencesTo(ailInitAddr).forEach(ref -> {
                if (ref.getReferenceType().isCall() || ref.getReferenceType().isData()) {
                    Function f = currentProgram.getFunctionManager()
                        .getFunctionContaining(ref.getFromAddress());
                    if (f != null) {
                        out.println("//   from " + ref.getFromAddress() + " in " + f.getName());
                        callerSet.add(f.getEntryPoint().getOffset());
                    }
                }
            });
            out.println();
            for (long addr : callerSet) {
                dumpAt(addr, decompiler, out);
            }
        }

        // Step 3: look for AIL callback registration functions
        // Miles uses AIL_register_sequence_callback / AIL_register_beat_callback etc.
        out.println("// === AIL callback registration functions ===");
        String[] callbackSymNames = {
            "_AIL_register_sequence_callback",
            "_AIL_register_beat_callback",
            "_AIL_register_EOB_callback",
            "_AIL_register_EOS_callback",
            "_AIL_sequence_callback",
            "AIL_register_sequence_callback",
        };
        for (String name : callbackSymNames) {
            SymbolIterator si = symTable.getSymbols(name);
            while (si.hasNext()) {
                Symbol sym = si.next();
                out.println("// Found: " + sym.getName() + " @ " + sym.getAddress());
                // find callers
                currentProgram.getReferenceManager().getReferencesTo(sym.getAddress()).forEach(ref -> {
                    Function f = currentProgram.getFunctionManager()
                        .getFunctionContaining(ref.getFromAddress());
                    if (f != null) out.println("//   caller: " + f.getName() + " @ " + f.getEntryPoint());
                });
            }
        }
        out.println();

        // Step 4: decompile MUS loader / FA.EXE MUS-reading functions
        // These are the functions that load MUS DLLs and pass them to AIL
        long[] musContextAddrs = {
            0x004A6AE0L, // FUN_004a6ae0 — loads HUD/MUS DLL by name (referenced in HUD init)
            0x004A6B50L, // likely variant or sequel to the above
            0x004A7180L, // near _SetupJT — may include MUS sequence setup
        };
        out.println("// === MUS/DLL loader context ===");
        for (long addr : musContextAddrs) {
            dumpAt(addr, decompiler, out);
        }

        out.close();
        decompiler.dispose();
        println("Done. Output written to C:\\Temp\\mus_callbacks.txt");
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
