import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class DumpDLGDispatch3 extends GhidraScript {

    private DecompInterface decompiler;
    private PrintWriter out;
    private Set<Long> dumped = new LinkedHashSet<>();

    @Override
    public void run() throws Exception {
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\dlg_dispatch3.txt");
        outFile.getParentFile().mkdirs();
        out = new PrintWriter(new FileWriter(outFile));

        // Goal:
        //   1. _DialogWhatItem@0 @ 0x488FC0 was NOT FOUND by Ghidra — force-create fn and decompile.
        //   2. FUN_00488470  — draw dispatcher called by _DialogShow for every record.
        //   3. FUN_00488490  — event dispatcher called by _ChooseActivity event loop.
        //   4. FUN_004a0810  — _ChoosePreload called by _ChooseActivity (filename param).
        //   5. FUN_0040bd30  — called before _DialogSetup with (filename, item_count, 0x28, 0, -1).
        //   6. FUN_00436280  — DLG module descriptor getter (returns width/height/ptr).
        //   7. _TopCenterDialog @ 0x489710 — NOT FOUND; force-create and decompile.
        //   8. FUN_004891a0  — special init called for type-7 records in _DialogShow.
        //   9. FUN_00488f50  — called by @DialogSelectItem with record+0x17 short.
        //  10. FUN_0048d0d0  — called with item index from _ChooseActivity; disable-item fn?
        //  11. FUN_0048d140  — called with item index; returns bool (enable check).

        long[] targets = {
            0x00488470L, // draw dispatcher (_DialogShow calls this for each record)
            0x00488490L, // event dispatcher (called in _ChooseActivity with input code)
            0x004a0810L, // _ChoosePreload (called with DLG filename)
            0x0040bd30L, // preload/init fn (filename, count, 0x28, 0, -1)
            0x00436280L, // DLG module descriptor getter
            0x004891a0L, // type-7 special init in _DialogShow
            0x00488f50L, // item-select helper called with record+0x17
            0x0048d0d0L, // disable-item fn (called with item index)
            0x0048d140L, // enable-check fn (returns bool; called with item index)
        };

        String[] names = {
            "FUN_00488470(draw_dispatch)",
            "FUN_00488490(event_dispatch)",
            "FUN_004a0810(_ChoosePreload?)",
            "FUN_0040bd30(preload_init)",
            "FUN_00436280(dlg_descriptor)",
            "FUN_004891a0(type7_init)",
            "FUN_00488f50(select_helper)",
            "FUN_0048d0d0(disable_item)",
            "FUN_0048d140(item_enabled?)",
        };

        for (int i = 0; i < targets.length; i++) {
            out.println("\n// === " + names[i] + " @ 0x" + Long.toHexString(targets[i]) + " ===");
            dumpAt(targets[i]);
        }

        // Force-create function at 0x488FC0 (_DialogWhatItem@0) and decompile.
        out.println("\n// === FORCE-CREATE: _DialogWhatItem@0 @ 0x488FC0 ===");
        dumpForced(0x00488FC0L);

        // Force-create function at 0x489710 (_TopCenterDialog) and decompile.
        out.println("\n// === FORCE-CREATE: _TopCenterDialog @ 0x489710 ===");
        dumpForced(0x00489710L);

        // Force-create function at 0x489580 (@DialogDeselectItem) and decompile.
        out.println("\n// === FORCE-CREATE: @DialogDeselectItem @ 0x489580 ===");
        dumpForced(0x00489580L);

        out.close();
        decompiler.dispose();
        println("Output: C:\\Temp\\dlg_dispatch3.txt");
    }

    private void dumpForced(long va) throws Exception {
        Address addr = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(va);
        Function fn = currentProgram.getFunctionManager().getFunctionAt(addr);
        if (fn == null) {
            out.println("// No fn at 0x" + Long.toHexString(va) + " — attempting disassemble+create");
            disassemble(addr);
            createFunction(addr, null);
            fn = currentProgram.getFunctionManager().getFunctionAt(addr);
        }
        if (fn == null) {
            out.println("// STILL NOT FOUND at 0x" + Long.toHexString(va));
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
