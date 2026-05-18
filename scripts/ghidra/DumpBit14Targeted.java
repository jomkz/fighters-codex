import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.scalar.*;
import java.io.*;
import java.util.*;

public class DumpBit14Targeted extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;
    private Set<Long> dumped = new LinkedHashSet<>();

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\bit14_targeted.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // HUD advisory bit 14 (0x4000) writer not yet identified.
        // DAT_0050cfef is at 0x0050cfef.
        // Strategy: find all writes to 0x0050cfef, then filter for those
        // involving the value 0x4000.
        //
        // Secondary: also look for the _PROJProc and CTDo_ functions by
        // trying getFunctionContaining at those SMS-listed addresses.

        // === Goal 1: find bit 14 writer ===
        // Find all functions that write to DAT_0050cfef.
        out.println("// === All references TO DAT_0050cfef (0x0050cfef) ===");
        Address hud_flags = toAddr(0x0050cfefL);
        Set<Long> writerFns = new LinkedHashSet<>();
        for (Reference ref : currentProgram.getReferenceManager().getReferencesTo(hud_flags)) {
            if (ref.getReferenceType().isWrite() || ref.getReferenceType().isData()
                    || ref.getReferenceType().isRead()) {
                Function fn = currentProgram.getFunctionManager()
                        .getFunctionContaining(ref.getFromAddress());
                if (fn != null && writerFns.add(fn.getEntryPoint().getOffset())) {
                    out.println("// " + ref.getReferenceType() + " @ " + ref.getFromAddress()
                            + " in " + fn.getName() + " @ " + fn.getEntryPoint());
                }
            }
        }
        out.println("// Total functions referencing DAT_0050cfef: " + writerFns.size());
        for (long va : writerFns) dumpAt(va);

        // === Goal 2: _PROJProc — try getting function containing the SMS address ===
        out.println("\n// === Trying getFunctionContaining for _PROJProc (0x004C1F50) ===");
        Address projProcAddr = toAddr(0x004C1F50L);
        Function projFn = currentProgram.getFunctionManager().getFunctionAt(projProcAddr);
        if (projFn == null) {
            projFn = currentProgram.getFunctionManager().getFunctionContaining(projProcAddr);
        }
        if (projFn != null) {
            out.println("// Found: " + projFn.getName() + " @ " + projFn.getEntryPoint());
            dumpAt(projFn.getEntryPoint().getOffset());
        } else {
            // Try the surrounding addresses
            out.println("// Not found at 0x4C1F50, trying 0x4C1F40-0x4C1F60");
            for (long va = 0x004C1F40L; va <= 0x004C1F60L; va += 4) {
                Function fn = currentProgram.getFunctionManager().getFunctionContaining(toAddr(va));
                if (fn != null) {
                    out.println("// Found via containing at 0x" + Long.toHexString(va)
                            + ": " + fn.getName() + " @ " + fn.getEntryPoint());
                    dumpAt(fn.getEntryPoint().getOffset());
                    break;
                }
            }
        }

        // === Goal 3: PROJMoveProc (0x004C11B0) ===
        out.println("\n// === Trying getFunctionContaining for PROJMoveProc (0x004C11B0) ===");
        Function moveFn = currentProgram.getFunctionManager().getFunctionContaining(toAddr(0x004C11B0L));
        if (moveFn != null) {
            out.println("// Found: " + moveFn.getName() + " @ " + moveFn.getEntryPoint());
            dumpAt(moveFn.getEntryPoint().getOffset());
        } else {
            out.println("// Not found");
        }

        // === Goal 4: GAS init — find where hardpoint fuel slot (+6) is initialized ===
        // The hardpoint fuel slot is `DAT_0050d31e + i * 0x18 + 6`.
        // Search for writes to that structure during aircraft loading.
        // Look for `_HARDPtrs@12` callers that also write to +6.
        out.println("\n// === Callers of _HARDPtrs@12 (0x00452770) ===");
        Address hardPtrsAddr = toAddr(0x00452770L);
        Set<Long> hardPtrsCallers = new LinkedHashSet<>();
        for (Reference ref : currentProgram.getReferenceManager().getReferencesTo(hardPtrsAddr)) {
            if (ref.getReferenceType().isCall()) {
                Function fn = currentProgram.getFunctionManager()
                        .getFunctionContaining(ref.getFromAddress());
                if (fn != null && hardPtrsCallers.add(fn.getEntryPoint().getOffset())) {
                    out.println("// CALLER: " + fn.getName() + " @ " + fn.getEntryPoint());
                }
            }
        }
        // Dump only the ones that look like initialization (not iterators like BurnFuel)
        for (long va : hardPtrsCallers) {
            if (va != 0x00451E80L && va != 0x00453A70L && va != 0x00452FF0L) {
                dumpAt(va);
            }
        }

        out.close();
        decompiler.dispose();
        println("Output: C:\\Temp\\bit14_targeted.txt");
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
