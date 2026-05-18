import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class DumpLAYStructure extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;
    private Set<Long> dumped = new LinkedHashSet<>();

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\lay_structure.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // Goal: resolve LAY.md TODO items:
        //   1. Confirm 0x31 and 10 10 in gradient sub-block header (+0x10B0 area):
        //      trace which function reads these bytes and what it does with them.
        //   2. Confirm sel_alt_min/sel_alt_max (+0x02/+0x06) vs alt_min/alt_max (+0x0A/+0x0E) units:
        //      FUN_004b3750 (CopyLayersToRuntime) selects by sel_alt; FUN_004b3be0 renders by alt_min/max.
        //   3. Confirm horizon_base_rgb (+0xFB) usage — trace T_DefaultHorizon (FUN_004aacf0).
        //   4. Header gap fields: dump ParseLayerFile (FUN_004b4370) to see what it does with
        //      header offsets +0x14, +0x18, +0x34..+0x44, +0x60..+0x68.
        //
        // All VAs from LAY.md confirmed function table:
        //   0x004b4370 ParseLayerFile
        //   0x004b3750 CopyLayersToRuntime
        //   0x004b3820 InterpolateLayers
        //   0x004b3be0 GetLayerAtAltitude
        //   0x004aacf0 T_DefaultHorizon
        //   0x004b3cb0 ApplyBrightnessGradient
        //   0x004b3d90 UpdateSkyState
        //   0x004b4170 UpdateAuroraClouds
        //   0x004b3ad0 FindNearestColorEntry
        //   0x004b3b60 LerpInt
        //   0x004b3b80 LerpRGB
        //   0x004b4680 LoadPICByWildcard
        //   0x004b3190 GetLayerBoundary

        long[] targets = {
            0x004b4370L, // ParseLayerFile — header loading; reads sub-block types
            0x004b3750L, // CopyLayersToRuntime — sel_alt selection
            0x004b3820L, // InterpolateLayers — alt-fraction blending
            0x004b3be0L, // GetLayerAtAltitude — rendering layer lookup
            0x004aacf0L, // T_DefaultHorizon — horizon_base_rgb usage
            0x004b3cb0L, // ApplyBrightnessGradient — brightness gradient
            0x004b3d90L, // UpdateSkyState — per-frame sky state
            0x004b4170L, // UpdateAuroraClouds — cloud/aurora update
            0x004b3ad0L, // FindNearestColorEntry — colour table search
            0x004b4680L, // LoadPICByWildcard — wildcard PIC loading
            0x004b3190L, // GetLayerBoundary — layer boundary search
        };

        String[] names = {
            "ParseLayerFile",
            "CopyLayersToRuntime",
            "InterpolateLayers",
            "GetLayerAtAltitude",
            "T_DefaultHorizon",
            "ApplyBrightnessGradient",
            "UpdateSkyState",
            "UpdateAuroraClouds",
            "FindNearestColorEntry",
            "LoadPICByWildcard",
            "GetLayerBoundary",
        };

        for (int i = 0; i < targets.length; i++) {
            out.println("\n// === " + names[i] + " @ 0x" + Long.toHexString(targets[i]) + " ===");
            dumpAt(targets[i]);
        }

        out.close();
        decompiler.dispose();
        println("Output: C:\\Temp\\lay_structure.txt");
    }

    private void dumpAt(long va) throws Exception {
        if (dumped.contains(va)) {
            out.println("// (already dumped)");
            return;
        }
        dumped.add(va);
        Address addr = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(va);
        Function fn = currentProgram.getFunctionManager().getFunctionAt(addr);
        if (fn == null) {
            out.println("// NOT FOUND at 0x" + Long.toHexString(va));
            return;
        }
        out.println("// --- " + fn.getName() + " ---");
        DecompileResults res = decompiler.decompileFunction(fn, 120, monitor);
        if (res != null && res.getDecompiledFunction() != null) {
            out.println(res.getDecompiledFunction().getC());
        } else {
            out.println("// decompile failed");
        }
    }
}
