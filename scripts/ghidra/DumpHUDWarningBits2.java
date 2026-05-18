import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class DumpHUDWarningBits2 extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\hud_warning_bits2.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // FUN_00407930: reads DAT_0050cfef bits 0-4 (damage indicators) and 28-31
        // (emergency/redacted display states). Goal: confirm which HUD element each
        // bit gates — sprite swap, text override, or tape visibility.
        out.println("// === FUN_00407930 (HUD warning-bit display dispatcher) ===");
        dumpAt(0x00407930L);

        // Also dump FUN_00407ee0 and FUN_00408420 which are known co-readers of
        // bit 28 (0x10000000) — redacted display gate.
        out.println("\n// === FUN_00407ee0 (speed/alt tape renderer — reads bit 28) ===");
        dumpAt(0x00407ee0L);

        out.println("\n// === FUN_00408420 (alt tape variant — reads bit 28) ===");
        dumpAt(0x00408420L);

        // FUN_00407a00: reads bit 5 (afterburner flag) and bit 12 (flight-lock).
        // Dump to cross-check the bit-5 and bit-12 rows already in HUD.md.
        out.println("\n// === FUN_00407a00 (throttle/G readout — reads bits 5,12) ===");
        dumpAt(0x00407a00L);

        out.close();
        decompiler.dispose();
        println("Output: C:\\Temp\\hud_warning_bits2.txt");
    }

    private void dumpAt(long va) throws Exception {
        Address addr = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(va);
        Function fn = currentProgram.getFunctionManager().getFunctionAt(addr);
        if (fn == null) {
            out.println("// NOT FOUND at 0x" + Long.toHexString(va));
            return;
        }
        DecompileResults res = decompiler.decompileFunction(fn, 60, monitor);
        if (res != null && res.getDecompiledFunction() != null) {
            out.println(res.getDecompiledFunction().getC());
        } else {
            out.println("// decompile failed: " + fn.getName());
        }
    }
}
