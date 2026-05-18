import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class DumpMCCAMLoader extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;
    private Set<Long> dumped = new LinkedHashSet<>();

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\mc_cam_loader.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // FUN_00428412 calls FUN_00481940(suffix, 0) to load the MC condition file.
        // Goal: trace FUN_00481940 to understand the MC file parser and binary layout.
        // Also find the CAM (UKRAINE.CAM) binary layout — mission state and weapon tables.

        out.println("// === FUN_00481940 (MC condition file loader) ===");
        dumpAt(0x00481940L);

        // Dump callers of FUN_00481940 to understand all MC load paths.
        out.println("\n// === Callers of FUN_00481940 ===");
        dumpCallers(0x00481940L);

        // FUN_00428412: campaign/mission launcher (already partial in mm_cam_mission.txt).
        out.println("\n// === FUN_00428412 (campaign launcher) ===");
        dumpAt(0x00428412L);

        // thunk_FUN_00480750: called before MC loading — likely CAM file loader or mission setup.
        out.println("\n// === thunk_FUN_00480750 (pre-MC setup) ===");
        dumpAt(0x00480750L);

        // FUN_004809d0: initializes hangar name (also called from MISSIONInit2).
        // FUN_00481940 may be loading .MC files — trace it to find binary sections.
        out.println("\n// === FA.SMS symbols matching MC/condition/campaign/CAM ===");
        for (Symbol sym : currentProgram.getSymbolTable().getAllSymbols(false)) {
            String name = sym.getName();
            String lower = name.toLowerCase();
            if (lower.contains("campaign") || lower.contains("_mc") || lower.contains("mc_")
                    || lower.contains("condition") || lower.contains("winlose")
                    || lower.contains("cond_") || lower.contains("_cam")
                    || name.contains("CAM") || lower.contains("scenario")) {
                out.println("// SYM: " + name + " @ " + sym.getAddress());
            }
        }

        // Also search near FUN_00481940 for related MC parsing functions.
        out.println("\n// === Functions near FUN_00481940 (0x481800-0x482200) ===");
        for (Function fn : currentProgram.getFunctionManager().getFunctions(true)) {
            long ep = fn.getEntryPoint().getOffset();
            if (ep >= 0x00481800L && ep <= 0x00482200L) {
                dumpAt(ep);
            }
        }

        out.close();
        decompiler.dispose();
        println("Output: C:\\Temp\\mc_cam_loader.txt");
    }

    private void dumpCallers(long targetVA) throws Exception {
        Address targetAddr = toAddr(targetVA);
        Set<Long> seen = new LinkedHashSet<>();
        for (Reference ref : currentProgram.getReferenceManager().getReferencesTo(targetAddr)) {
            if (ref.getReferenceType().isCall()) {
                Function fn = currentProgram.getFunctionManager()
                        .getFunctionContaining(ref.getFromAddress());
                if (fn != null && seen.add(fn.getEntryPoint().getOffset())) {
                    out.println("// CALLER: " + fn.getName() + " @ " + fn.getEntryPoint());
                }
            }
        }
        for (long va : seen) dumpAt(va);
    }

    private void dumpAt(long va) throws Exception {
        if (dumped.contains(va)) return;
        dumped.add(va);
        Address addr = toAddr(va);
        Function fn = currentProgram.getFunctionManager().getFunctionAt(addr);
        if (fn == null) fn = currentProgram.getFunctionManager().getFunctionContaining(addr);
        if (fn == null) {
            out.println("// NOT FOUND at 0x" + Long.toHexString(va));
            return;
        }
        out.println("// --- " + fn.getName() + " @ 0x" + Long.toHexString(va) + " ---");
        DecompileResults res = decompiler.decompileFunction(fn, 120, monitor);
        if (res != null && res.getDecompiledFunction() != null) {
            out.println(res.getDecompiledFunction().getC());
        } else {
            out.println("// decompile failed: " + fn.getName());
        }
        out.println();
    }
}
