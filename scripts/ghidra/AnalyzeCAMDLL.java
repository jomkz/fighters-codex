// Campaign DLL binary layout analysis.
// Runs against %FA_PROJECT%\overlay_projects\cam\
// For each CAM DLL: dumps all exported functions, then scans the .data section
// for the mission-state block (null-terminated strings, aircraft IDs, weapon tables).
// Also traces FUN_00428340 (post-init finalizer) to determine its role.
//
// Invoke via run_overlays.bat --analyze CAM
// Output: %FA_PROJECT%\output\AnalyzeCAMDLL.txt

import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class AnalyzeCAMDLL extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;
    private Set<Long> dumped;

    @Override
    public void run() throws Exception {
        String projectDir = System.getenv("FA_PROJECT");
        if (projectDir == null || projectDir.isEmpty())
            projectDir = System.getProperty("java.io.tmpdir");
        File outDir = new File(projectDir, "output");
        outDir.mkdirs();
        File outFile = new File(outDir, "AnalyzeCAMDLL.txt");

        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        dumped = new LinkedHashSet<>();
        out = new PrintWriter(new FileWriter(outFile, true));  // append: one file per project run

        out.println("// ============================================================");
        out.println("// AnalyzeCAMDLL -- campaign DLL binary layout");
        out.println("// Program: " + currentProgram.getName());
        out.println("// Image base: 0x" + Long.toHexString(
                currentProgram.getImageBase().getOffset()));
        out.println("// ============================================================");
        out.println();

        // -----------------------------------------------------------------------
        // Exported functions
        // -----------------------------------------------------------------------
        header("CAM -- exported entry points");
        SymbolTable st = currentProgram.getSymbolTable();
        FunctionManager fm = currentProgram.getFunctionManager();
        for (Symbol sym : st.getAllSymbols(false)) {
            if (sym.isExternalEntryPoint()) {
                Function fn = fm.getFunctionAt(sym.getAddress());
                if (fn != null) {
                    out.println("// EXPORT: " + sym.getName() + " @ " + sym.getAddress());
                    dumpAt(fn.getEntryPoint().getOffset());
                }
            }
        }

        // -----------------------------------------------------------------------
        // All functions (VA order) -- covers any non-exported helpers
        // -----------------------------------------------------------------------
        header("CAM -- all functions (VA order)");
        List<Function> allFns = new ArrayList<>();
        for (Function fn : fm.getFunctions(true)) allFns.add(fn);
        allFns.sort((a, b) -> Long.compare(
                a.getEntryPoint().getOffset(), b.getEntryPoint().getOffset()));
        for (Function fn : allFns) {
            dumpAt(fn.getEntryPoint().getOffset());
        }

        // -----------------------------------------------------------------------
        // Post-init finalizer (known VA in FA.EXE -- may appear as import thunk)
        // -----------------------------------------------------------------------
        header("CAM -- post-init finalizer FUN_00428340 (if present)");
        long finalizerVa = 0x00428340L;
        Function fin = fm.getFunctionAt(toAddr(finalizerVa));
        if (fin != null) {
            dumpAt(finalizerVa);
        } else {
            out.println("// FUN_00428340 not present in this overlay (expected -- it is in FA.EXE)");
        }

        // -----------------------------------------------------------------------
        // Mission-state data block scan in .data section
        // -----------------------------------------------------------------------
        header("CAM -- mission-state data block scan (null-terminated strings, IDs)");
        scanMissionStateBlock();

        out.println("\n// Total functions dumped: " + dumped.size());
        out.close();
        decompiler.dispose();
        println("AnalyzeCAMDLL complete -> " + outFile.getAbsolutePath());
    }

    private void scanMissionStateBlock() throws Exception {
        Memory mem = currentProgram.getMemory();
        // Scan all initialized blocks including the CODE section -- CAM mission string
        // tables (mission names, aircraft IDs, weapon filenames) live in the executable
        // CODE section at known offsets (e.g. DAT_00001594 for UKRAINE.CAM), so we
        // cannot skip executable blocks.
        for (MemoryBlock block : mem.getBlocks()) {
            if (!block.isInitialized()) continue;
            if (block.getSize() < 16) continue;

            out.println("// Scanning block: " + block.getName()
                    + " 0x" + Long.toHexString(block.getStart().getOffset())
                    + " - 0x" + Long.toHexString(block.getEnd().getOffset())
                    + " (" + block.getSize() + " bytes)");

            byte[] data = new byte[(int) Math.min(block.getSize(), 4096)];
            block.getBytes(block.getStart(), data);

            // Find null-terminated ASCII strings of length 4-64
            int i = 0;
            while (i < data.length) {
                int start = i;
                while (i < data.length && data[i] != 0 && data[i] >= 0x20 && data[i] < 0x7F) i++;
                int len = i - start;
                if (len >= 4 && len <= 64 && i < data.length && data[i] == 0) {
                    String s = new String(data, start, len, "US-ASCII");
                    long va = block.getStart().getOffset() + start;
                    out.println("  0x" + Long.toHexString(va) + "  \"" + s + "\"");
                }
                i++;
            }
        }
    }

    private void header(String title) {
        out.println("\n// === " + title + " ===");
    }

    private void dumpAt(long va) throws Exception {
        if (dumped.contains(va)) return;
        dumped.add(va);
        FunctionManager fm = currentProgram.getFunctionManager();
        Function fn = fm.getFunctionAt(toAddr(va));
        if (fn == null) fn = fm.getFunctionContaining(toAddr(va));
        if (fn == null) {
            out.println("// NOT FOUND at 0x" + Long.toHexString(va));
            return;
        }
        out.println("// --- " + fn.getName() + " @ " + fn.getEntryPoint() + " ---");
        DecompileResults res = decompiler.decompileFunction(fn, 120, monitor);
        if (res != null && res.getDecompiledFunction() != null) {
            out.println(res.getDecompiledFunction().getC());
        } else {
            out.println("// decompile failed: " + fn.getName());
        }
        out.println();
    }
}
