// SH interpreter dispatch table recovery (epic #52 groundwork for #122/#123).
//
// The shape interpreter is hand-written threaded code: every handler ends with
//     mov ax,[esi] / lea esi,[esi+2] / movzx ebx,al / jmp [vector_table + ebx*2]
// so Ghidra's auto-analysis never created functions at the handler entries.
// This script reads vector_table (FA.SMS symbol, 0x5183A0 in .data): 128 dword
// entries addressed at table + opcode*2 — only even opcodes dispatch cleanly.
// For each entry it disassembles the target, creates+labels a function
// (sh_op_XX or the FA.SMS do_* name when present), and dumps the full map.

import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;

public class AnalyzeSHDispatch extends FAScript {

    private static final long VECTOR_TABLE = 0x5183A0L;
    private static final int ENTRIES = 128;

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeSHDispatch");

        header("vector_table (0x5183a0) — opcode -> handler map");
        out.println("// op    handler    symbol");
        for (int k = 0; k < ENTRIES; k++) {
            int op = k * 2;
            long target = 0;
            Address slot = toAddr(VECTOR_TABLE + 4L * k);
            target = getInt(slot) & 0xFFFFFFFFL;
            Address h = toAddr(target);

            // ensure the handler is disassembled and wrapped in a function
            if (getInstructionAt(h) == null) {
                new DisassembleCommand(h, null, true).applyTo(currentProgram, monitor);
            }
            Function fn = getFunctionAt(h);
            Symbol sym = getSymbolAt(h);
            String name = (sym != null) ? sym.getName() : null;
            if (fn == null && getInstructionAt(h) != null) {
                fn = createFunction(h, name != null ? name : String.format("sh_op_%02X", op));
            }
            // shared handlers serve several slots — name them once, don't ping-pong
            if (fn != null && name == null && fn.getName().startsWith("FUN_")) {
                fn.setName(String.format("sh_op_%02X", op), SourceType.USER_DEFINED);
            }
            out.println(String.format("// 0x%02X  0x%x  %s", op, target,
                    name != null ? name : (fn != null ? fn.getName() : "(undisassemblable)")));
        }

        header("Handler disassembly — animation & LOD/damage focus (#122/#123)");
        // Threaded handlers end in jmp [vector_table+ebx*2] and often have no
        // conventional prologue, so the decompiler emits nothing useful — dump
        // the raw listing instead, stopping after the dispatch jump.
        // 0x40 JumpToFrame, 0x48 Jump, 0xA6 JumpToDetail, 0xAC JumpToDamage,
        // 0xC8 JumpToLOD, 0x38/0x50 ijmp family
        long[] focus = { 0x4d3134L, 0x4d30e5L, 0x4d2318L, 0x4d22d4L, 0x4d416cL,
                         0x4d30e4L, 0x4d3100L };
        for (long va : focus) {
            Symbol s = getSymbolAt(toAddr(va));
            out.println("// --- " + (s != null ? s.getName() : "?")
                    + " @ 0x" + Long.toHexString(va) + " ---");
            Address a = toAddr(va);
            for (int n = 0; n < 60; n++) {
                ghidra.program.model.listing.Instruction ins = getInstructionAt(a);
                if (ins == null) {
                    new DisassembleCommand(a, null, true).applyTo(currentProgram, monitor);
                    ins = getInstructionAt(a);
                    if (ins == null) { out.println("//   (undisassemblable)"); break; }
                }
                out.println("//   " + a + "  " + ins);
                String t = ins.toString();
                if (t.startsWith("JMP dword ptr [EBX*") || t.startsWith("RET")) break;
                a = ins.getMaxAddress().add(1);
            }
            out.println();
        }

        closeOutput();
    }
}
