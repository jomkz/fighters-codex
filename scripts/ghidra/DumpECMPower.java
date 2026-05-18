import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.scalar.*;
import java.io.*;
import java.util.*;

public class DumpECMPower extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;
    private Set<Long> dumped = new LinkedHashSet<>();

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\ecm_power.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // Goal: confirm semantics of ECM power word bits 5-7 (0x20/0x40/0x80).
        // Confirmed: bit 4 (0x10) = radar jammer, bit 8 (0x100) = IR jammer via
        // @HARDFindJammer@4. ECM power word is at ECM entity+0x08.
        // Power values in BRF files: $0=0, $f0=0xF0 (bits 4-7), $1f0=0x1F0 (bits 4-8).
        // Search for tests of 0x20/0x40/0x80 in the ECM/jammer code range (0x45xxxx range
        // where HARDFindJammer lives, plus broad scan for ECM power reads).

        out.println("// === @HARDFindJammer@4 (0x00452EA0) ===");
        dumpAt(0x00452EA0L);

        // FUN_00452980 was confirmed as the effectiveness reader. Dump it for context.
        out.println("\n// === FUN_00452980 (ECM effectiveness reader) ===");
        dumpAt(0x00452980L);

        // Search for tests of 0x20, 0x40, 0x80 near known ECM functions (0x452000-0x453000)
        out.println("\n// === Bit tests 0x20/0x40/0x80 in ECM range 0x452000-0x454000 ===");
        int[] masks = {0x20, 0x40, 0x80, 0xe0};
        for (int mask : masks) {
            out.println("// -- mask 0x" + Integer.toHexString(mask) + " --");
            searchBitTestsInRange(0x00452000L, 0x00454000L, mask);
        }

        // Also search in the broader jammer/ECM dispatch area
        out.println("\n// === Bit tests 0x20/0x40/0x80 in 0x004b0000-0x004c0000 ===");
        for (int mask : masks) {
            out.println("// -- mask 0x" + Integer.toHexString(mask) + " --");
            searchBitTestsInRange(0x004b0000L, 0x004c0000L, mask);
        }

        out.close();
        decompiler.dispose();
        println("Output: C:\\Temp\\ecm_power.txt");
    }

    private void dumpAt(long va) throws Exception {
        if (dumped.contains(va)) return;
        dumped.add(va);
        Address addr = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(va);
        Function fn = currentProgram.getFunctionManager().getFunctionAt(addr);
        if (fn == null) {
            out.println("// NOT FOUND at 0x" + Long.toHexString(va));
            return;
        }
        out.println("// --- " + fn.getName() + " @ 0x" + Long.toHexString(va) + " ---");
        DecompileResults res = decompiler.decompileFunction(fn, 60, monitor);
        if (res != null && res.getDecompiledFunction() != null) {
            out.println(res.getDecompiledFunction().getC());
        } else {
            out.println("// decompile failed: " + fn.getName());
        }
    }

    private void searchBitTestsInRange(long start, long end, int mask) throws Exception {
        AddressSpace space = currentProgram.getAddressFactory().getDefaultAddressSpace();
        Listing listing = currentProgram.getListing();
        Address startAddr = space.getAddress(start);
        Address endAddr = space.getAddress(end);

        InstructionIterator instrs = listing.getInstructions(startAddr, true);
        while (instrs.hasNext()) {
            Instruction instr = instrs.next();
            if (instr.getAddress().compareTo(endAddr) > 0) break;
            String mnem = instr.getMnemonicString().toLowerCase();
            if (!mnem.equals("and") && !mnem.equals("test")) continue;
            for (int i = 0; i < instr.getNumOperands(); i++) {
                Object[] objs = instr.getOpObjects(i);
                for (Object obj : objs) {
                    if (obj instanceof Scalar) {
                        long val = ((Scalar) obj).getUnsignedValue();
                        if (val == mask) {
                            Function fn = currentProgram.getFunctionManager()
                                    .getFunctionContaining(instr.getAddress());
                            String fnName = fn != null ? fn.getName() : "unknown";
                            out.println("  " + instr.getAddress() + "  "
                                    + instr.getMnemonicString() + "  "
                                    + instr.getDefaultOperandRepresentation(0)
                                    + ", " + instr.getDefaultOperandRepresentation(1)
                                    + "  [in " + fnName + "]");
                        }
                    }
                }
            }
        }
    }
}
