import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class DumpMMCAMMission extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;
    private Set<Long> dumped = new LinkedHashSet<>();

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\mm_cam_mission.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // === Goal 1: MM world-space coordinate scale and obj flags bits 9-10 ===
        // FUN_0047a510 is the MM keyword handler (from mm_layer_slot4.txt).
        // Need: pos/view coordinate scale, obj flags bit 9 (mission-critical?),
        //       obj flags bit 10 (friendly vs hostile?), tdic id=256 meaning.
        out.println("// === FUN_0047a510 (MM keyword handler) @ 0x0047a510 ===");
        dumpAt(0x0047a510L);

        // Dump callers of FUN_0047a510 to find the MM keyword dispatch loop.
        out.println("\n// === Callers of FUN_0047a510 ===");
        dumpCallers(0x0047a510L);

        // Functions near 0x004750-0x004800 range for MM/map loading.
        out.println("\n// === FA.SMS symbols matching MM/map/mission/layer ===");
        for (Symbol sym : currentProgram.getSymbolTable().getAllSymbols(false)) {
            String name = sym.getName().toLowerCase();
            if (name.contains("_mm") || name.contains("mm_") || name.contains("maplayer")
                    || name.contains("layerfile") || name.contains("layer_")) {
                out.println("// SYM: " + sym.getName() + " @ " + sym.getAddress());
            }
        }

        // FUN_004b4370 is ParseLayerFile (confirmed from mm_layer_slot.txt).
        // Dump it and its callers to understand the MM binary layout.
        out.println("\n// === FUN_004b4370 (ParseLayerFile) ===");
        dumpAt(0x004b4370L);

        // FUN_004ace50 and FUN_004acfa0 are referenced in the MM keyword handler.
        out.println("\n// === FUN_004ace50 (keyword lookup) ===");
        dumpAt(0x004ace50L);

        out.println("\n// === FUN_004acfa0 (keyword value reader) ===");
        dumpAt(0x004acfa0L);

        // === Goal 2: CAM binary layout ===
        // Need: confirm binary layout of UKRAINE.CAM — mission state and weapon tables.
        // _MISSIONInit2@0 @ 0x00480B50 loads the campaign/mission state.
        out.println("\n// === _MISSIONInit2@0 @ 0x00480B50 ===");
        dumpAt(0x00480B50L);

        out.println("\n// === ?_MISSIONInit2@@YGXXZ @ 0x00480A30 ===");
        dumpAt(0x00480A30L);

        // Search for CAM/campaign loader symbols.
        out.println("\n// === FA.SMS symbols matching CAM/campaign/mission ===");
        for (Symbol sym : currentProgram.getSymbolTable().getAllSymbols(false)) {
            String name = sym.getName().toLowerCase();
            if (name.contains("campaign") || name.contains("_cam") || name.contains("cam_")
                    || (name.contains("mission") && !name.contains("missioninit"))
                    || name.contains("weapon") && name.contains("table")) {
                out.println("// SYM: " + sym.getName() + " @ " + sym.getAddress());
            }
        }

        // Dump callers of _MISSIONInit2 to find the campaign loader.
        out.println("\n// === Callers of _MISSIONInit2@0 (0x00480B50) ===");
        dumpCallers(0x00480B50L);

        // === Goal 3: MC condition check logic ===
        // MC files contain mission condition logic. Look for MC loader symbols.
        out.println("\n// === FA.SMS symbols matching MC/condition/scenario ===");
        for (Symbol sym : currentProgram.getSymbolTable().getAllSymbols(false)) {
            String name = sym.getName().toLowerCase();
            if (name.contains("_mc") || name.contains("mc_") || name.contains("condition")
                    || name.contains("scenario") || name.contains("winlose")
                    || name.contains("win_") || name.contains("_lose")) {
                out.println("// SYM: " + sym.getName() + " @ " + sym.getAddress());
            }
        }

        out.close();
        decompiler.dispose();
        println("Output: C:\\Temp\\mm_cam_mission.txt");
    }

    private void dumpCallers(long targetVA) throws Exception {
        Address targetAddr = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(targetVA);
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
