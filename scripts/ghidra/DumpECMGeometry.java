import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import java.io.*;
import java.util.*;

public class DumpECMGeometry extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\ecm_geometry.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // FUN_004d5e58: ECM seeker-defeat / beam-geometry calculation.
        // Called as FUN_004d5e58(local_14, bVar1<<8, bVar2<<8, 0) from FUN_004c39a0.
        // The six fixed constants (35/95/24 radar, 35/159/31 IR) are jammer
        // effectiveness + beam-geometry params. We need the axis meanings
        // and the ECM power word bits 5–7.
        out.println("// === FUN_004d5e58 (ECM seeker-defeat / beam geometry) ===");
        dumpAt(0x004d5e58L);

        // FUN_004c39a0: calls FUN_004d5e58 with shifted ECM bytes.
        // This is where the byte-shift left-8 happens — dump it to see
        // which ECM struct fields it reads (power word, beam params).
        out.println("// === FUN_004c39a0 (ECM effectiveness caller) ===");
        dumpAt(0x004c39a0L);

        // Dump all callers of FUN_004d5e58 to get the full call graph.
        out.println("// === All callers of FUN_004d5e58 ===");
        Address ecmAddr = toAddr(0x004d5e58L);
        Set<Long> callerAddrs = new LinkedHashSet<>();
        for (Reference ref : currentProgram.getReferenceManager().getReferencesTo(ecmAddr)) {
            if (ref.getReferenceType().isCall()) {
                Function f = currentProgram.getFunctionManager()
                    .getFunctionContaining(ref.getFromAddress());
                if (f != null) callerAddrs.add(f.getEntryPoint().getOffset());
            }
        }
        out.println("// Callers found: " + callerAddrs.size());
        for (long addr : callerAddrs) {
            if (addr != 0x004c39a0L) { // already dumped above
                dumpAt(addr);
            }
        }

        // @HARDFindJammer@4: jammer lookup — reads ECM struct to find active jammer.
        // Confirms ECM power word structure and bit-field layout.
        out.println("// === @HARDFindJammer@4 ===");
        for (Symbol sym : currentProgram.getSymbolTable().getSymbols("@HARDFindJammer@4")) {
            dumpAt(sym.getAddress().getOffset());
        }

        out.close();
        decompiler.dispose();
        println("Done. Output written to C:\\Temp\\ecm_geometry.txt");
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
