import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class DumpDLGDispatch2 extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;
    private Set<Long> dumped = new LinkedHashSet<>();

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\dlg_dispatch2.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // Goal: confirm DLG common header gap +0x02..+0x09 role and decode
        // _ChoosePreload four-i16 params.
        //
        // VAs from FA.SMS:
        //   _DialogWhatItem@0      0x00488FC0  — hit-test: which item was clicked
        //   _DialogSetup@12        0x00487A63  — dialog setup (3 params)
        //   _DialogShow@0          0x004880D0  — shows dialog
        //   _DialogShutDown@8      0x00488190  — shuts down dialog
        //   _DialogDone@0          0x00488300  — dialog done handler
        //   _ChooseActivity@0      0x004A08A0  — main activity dispatcher
        //   @DialogGetPtr@4        0x004892E0  — get dialog item pointer
        //   @DialogGetValue@4      0x00489300  — get dialog item value
        //   @DialogSelectItem@4    0x004894F0  — select item
        //   @DialogDeselectItem@4  0x00489580  — deselect item
        //   _DrawAction            0x00489B90  — button draw function
        //   _DrawText              0x00489AC0  — text draw function
        //   _DrawRocker            0x0048B4E0  — rocker draw function
        //   _DrawFormattedText     0x0048A910  — formatted text draw
        //   _DrawEditBox           0x0048C710  — edit box draw
        //   _TopCenterDialog       0x00489710  — center dialog on screen

        long[] targets = {
            0x00488FC0L, // _DialogWhatItem@0
            0x00487A63L, // _DialogSetup@12
            0x004880D0L, // _DialogShow@0
            0x00488190L, // _DialogShutDown@8
            0x00488300L, // _DialogDone@0
            0x004A08A0L, // _ChooseActivity@0
            0x004892E0L, // @DialogGetPtr@4
            0x00489300L, // @DialogGetValue@4
            0x004894F0L, // @DialogSelectItem@4
            0x00489580L, // @DialogDeselectItem@4
            0x00489B90L, // _DrawAction
            0x00489AC0L, // _DrawText
            0x0048B4E0L, // _DrawRocker
            0x0048A910L, // _DrawFormattedText
            0x0048C710L, // _DrawEditBox
            0x00489710L, // _TopCenterDialog
        };

        String[] names = {
            "_DialogWhatItem", "_DialogSetup", "_DialogShow", "_DialogShutDown",
            "_DialogDone", "_ChooseActivity", "@DialogGetPtr", "@DialogGetValue",
            "@DialogSelectItem", "@DialogDeselectItem", "_DrawAction", "_DrawText",
            "_DrawRocker", "_DrawFormattedText", "_DrawEditBox", "_TopCenterDialog"
        };

        for (int i = 0; i < targets.length; i++) {
            out.println("\n// === " + names[i] + " @ 0x" + Long.toHexString(targets[i]) + " ===");
            dumpAt(targets[i]);
        }

        out.close();
        decompiler.dispose();
        println("Output: C:\\Temp\\dlg_dispatch2.txt");
    }

    private void dumpAt(long va) throws Exception {
        if (dumped.contains(va)) {
            out.println("// (already dumped) 0x" + Long.toHexString(va));
            return;
        }
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
}
