// Campaign DLL binary layout analysis.
// Runs against %FA_PROJECT%\overlay_projects\cam\
// For each CAM DLL: dumps all exported functions, then scans the .data section
// for the mission-state block (null-terminated strings, aircraft IDs, weapon tables).
// Also traces FUN_00428340 (post-init finalizer) to determine its role.
//
// Invoke via run_overlays.bat --analyze CAM
// Output: %FA_PROJECT%\output\AnalyzeCAMDLL.txt

import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;
import java.util.*;

public class AnalyzeCAMDLL extends FAScript {

    @Override
    public void run() throws Exception {
        openOutputAppend("AnalyzeCAMDLL");

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
        closeOutput();
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
}
