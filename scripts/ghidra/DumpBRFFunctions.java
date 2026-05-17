import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import java.io.*;

public class DumpBRFFunctions extends GhidraScript {

    // BRF-related evaluation functions: ECM, SEE dual-lobe, JT warhead/Pk
    static final long[] ADDRESSES = {
        // ECM
        0x00452F10L, // @HARDFindECMForObj@8 — reads ECM struct, applies jamming
        0x00452EA0L, // @HARDFindJammer@4 — locates active jammer pod on obj

        // SEE / seeker lobe
        0x00452E60L, // @HARDBestSeeker@8 — selects best seeker against a target
        0x00452D90L, // @HARDBestSeekers@12
        0x004C2F20L, // _PROJLock@24 — seeker lock; dual-lobe switch expected here
        0x004C0960L, // _PROJLockUpdate@0 — ongoing lock/track update
        0x004AD090L, // _Seek@12 — seeker eval (search/track modes)
        0x004AD000L, // _SeekTell@12 — query seek state
        0x0047A090L, // _LibSeek@8 — lower-level seek routine
        0x004650E0L, // _CTEval_ir — AI IR launch condition evaluator
        0x004650A0L, // _CTEval_radar — AI radar launch condition evaluator
        0x00464E70L, // _CTEval_do_ir_launch
        0x00464E60L, // _CTEval_do_radar_launch

        // JT / warhead
        0x004A7230L, // _SetupJT — initialises JT struct; likely reads flag bits
        0x004A6EB0L, // func called by _SetupJT — actual JT init body
        0x004C1870L, // ?PROJDamageProc — checks warhead flags on hit
        0x0040F970L, // _DAMAGEDoHit@12 — applies damage; uses warhead type

        // SEE lobe check functions (found via _PROJLock@24)
        0x004C2EB0L, // FUN_004c2eb0 — lobe FOV check A (called when flag 0x10000)
        0x004C31F0L, // FUN_004c31f0 — lobe FOV check B (called when flag 0x20000)
        0x004C2860L, // _PROJInFOV@40 — general FOV check used in lock path

        // ECM bytes 1/5 and JT warhead flags
        0x004C2B50L, // FUN_004c2b50 — base Pk/range calc called before ECM; may read eff_A/eff_B
        0x004C3360L, // FUN_004c3360 — checks IR/guidance property (called in _PROJHitChance)
        0x004C20C0L, // _PROJHit@8 — hit event; warhead flags checked here
        0x004C5670L, // _PROJSendCollateralDamages@24 — blast damage
        0x004C3960L, // FUN near _PROJServiceWeapon — proximity/impact fuze
    };

    @Override
    public void run() throws Exception {
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        File outFile = new File("C:\\Temp\\brf_functions.txt");
        outFile.getParentFile().mkdirs();
        PrintWriter out = new PrintWriter(new FileWriter(outFile));

        for (long addr : ADDRESSES) {
            Address funcAddr = currentProgram.getAddressFactory()
                .getDefaultAddressSpace().getAddress(addr);
            dumpAt(funcAddr, decompiler, out);
        }

        out.close();
        decompiler.dispose();
        println("Done. Output written to C:\\Temp\\brf_functions.txt");
    }

    private void dumpAt(Address funcAddr, DecompInterface decompiler, PrintWriter out) throws Exception {
        Function func = getFunctionAt(funcAddr);
        if (func == null) func = currentProgram.getFunctionManager().getFunctionContaining(funcAddr);
        if (func == null) {
            DisassembleCommand disCmd = new DisassembleCommand(funcAddr, null, true);
            disCmd.applyTo(currentProgram, monitor);
            CreateFunctionCmd createCmd = new CreateFunctionCmd(funcAddr);
            createCmd.applyTo(currentProgram, monitor);
            func = getFunctionAt(funcAddr);
        }
        if (func != null) {
            DecompileResults results = decompiler.decompileFunction(func, 120, monitor);
            if (results.decompileCompleted()) {
                out.println("// === " + func.getName() + " @ " + func.getEntryPoint() + " ===");
                out.println(results.getDecompiledFunction().getC());
                out.println();
            } else {
                out.println("// === DECOMPILE FAILED: " + funcAddr + " ===\n");
            }
        } else {
            out.println("// === NO FUNCTION AT: " + funcAddr + " ===\n");
        }
    }
}
