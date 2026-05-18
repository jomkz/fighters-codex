import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class DumpGVProcGAS extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;
    private Set<Long> dumped = new LinkedHashSet<>();

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\gvproc_gas.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // Goal 1: NT hardpoint bit 1 ($2) — fire-control dispatcher trace.
        // _GVProc @ 0x00473DB0 is the vehicle/ship AI dispatcher.
        // Also: _HARDPtrs@12, HARDPtrsFort, _HARDFindProj@16, @HardpointAngle@4.
        out.println("// === _GVProc @ 0x00473DB0 ===");
        dumpAt(0x00473DB0L);

        out.println("\n// === _HARDPtrs@12 @ 0x00452770 ===");
        dumpAt(0x00452770L);

        out.println("\n// === ?HARDPtrsFort @ 0x00452870 ===");
        dumpAt(0x00452870L);

        out.println("\n// === _HARDFindProj@16 @ 0x00452FF0 ===");
        dumpAt(0x00452FF0L);

        out.println("\n// === @HardpointAngle@4 @ 0x004AB7F0 ===");
        dumpAt(0x004AB7F0L);

        // Dump callers of _GVProc to understand the vehicle update loop.
        out.println("\n// === Callers of _GVProc (0x00473DB0) ===");
        Address gvAddr = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(0x00473DB0L);
        for (Reference ref : currentProgram.getReferenceManager().getReferencesTo(gvAddr)) {
            if (ref.getReferenceType().isCall()) {
                Function fn = currentProgram.getFunctionManager()
                        .getFunctionContaining(ref.getFromAddress());
                if (fn != null) dumpAt(fn.getEntryPoint().getOffset());
            }
        }

        // Goal 2: GAS capacity word unit — trace the fuel pool builder.
        // The GAS word field (108/198/248/315) is read during aircraft init to set fuel pool.
        // Entry points confirmed from FA.SMS:
        //   @FMFuelConsumption@4 @ 0x00451E50
        //   _BurnFuel@0         @ 0x00451E80
        //   @FMBurnNPCFuel@4    @ 0x00452050
        //   _HARDTotalFuel@0    @ 0x00453A70  (called from _PLANECheckFuel / FUN_0049FB70)
        out.println("\n// === @FMFuelConsumption@4 @ 0x00451E50 ===");
        dumpAt(0x00451E50L);

        out.println("\n// === _BurnFuel@0 @ 0x00451E80 ===");
        dumpAt(0x00451E80L);

        out.println("\n// === @FMBurnNPCFuel@4 @ 0x00452050 ===");
        dumpAt(0x00452050L);

        out.println("\n// === _HARDTotalFuel@0 @ 0x00453A70 ===");
        dumpAt(0x00453A70L);

        // Dump callers of _BurnFuel to find fuel pool initialization.
        out.println("\n// === Callers of _BurnFuel@0 (0x00451E80) ===");
        Address burnAddr = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(0x00451E80L);
        Set<Long> callersSeen = new LinkedHashSet<>();
        for (Reference ref : currentProgram.getReferenceManager().getReferencesTo(burnAddr)) {
            if (ref.getReferenceType().isCall()) {
                Function fn = currentProgram.getFunctionManager()
                        .getFunctionContaining(ref.getFromAddress());
                if (fn != null && callersSeen.add(fn.getEntryPoint().getOffset())) {
                    out.println("// CALLER: " + fn.getName() + " @ " + fn.getEntryPoint());
                }
            }
        }
        for (long va : callersSeen) dumpAt(va);

        // ?MPSetFuel@@YIXG@Z @ 0x004723A0 — multiplayer set fuel (may show unit conversion).
        out.println("\n// === ?MPSetFuel@@YIXG@Z @ 0x004723A0 ===");
        dumpAt(0x004723A0L);

        // Also dump all FA.SMS symbols matching fuel/gas to get the full picture.
        out.println("\n// === All fuel/GAS symbols ===");
        for (Symbol sym : currentProgram.getSymbolTable().getAllSymbols(false)) {
            String name = sym.getName().toLowerCase();
            if (name.contains("fuel") || name.contains("gas") || name.contains("tank")) {
                out.println("// SYM: " + sym.getName() + " @ " + sym.getAddress());
            }
        }

        out.close();
        decompiler.dispose();
        println("Output: C:\\Temp\\gvproc_gas.txt");
    }

    private void dumpAt(long va) throws Exception {
        if (dumped.contains(va)) return;
        dumped.add(va);
        Address addr = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(va);
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
