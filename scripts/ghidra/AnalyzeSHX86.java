// SH X86Unknown region entry contract (issue #125).
//
// The 0xF0 X86Code opcode hands control from the threaded bytecode interpreter
// to raw x86 embedded in the shape. This script dumps the FA.EXE side of that
// contract via Ghidra's structured listing (no payload disassembly):
//   - do_start_interp (0x4D4240): the bytecode entry the x86 blocks call back
//     into (via an FF25 trampoline) to resume interpreting a chosen sub-stream.
//   - do_start_asm    (0x4D4254): the 0xF0 handler that transfers to the x86.
//   - do_collision_info (0x4D4258): the 0xF2 PtrToObjEnd handler bounding the
//     region.
// Also decompiles GRAddBrentObj (0x4D057C) for the register/state context the
// x86 inherits.

import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.symbol.Symbol;

public class AnalyzeSHX86 extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeSHX86");

        header("Raw listing 0x4D4240-0x4D4290 (do_start_interp / do_start_asm / do_collision_info)");
        dumpListing(0x4D4240L, 0x4D4290L);

        header("do_start_interp (0x4D4240) — decompile");
        dumpAtForced(0x4D4240L);

        header("do_start_asm (0x4D4254) — decompile");
        dumpAtForced(0x4D4254L);

        header("do_collision_info (0x4D4258) — decompile");
        dumpAtForced(0x4D4258L);

        closeOutput();
    }

    private void dumpListing(long start, long end) throws Exception {
        AddressSet set = new AddressSet(toAddr(start), toAddr(end - 1));
        InstructionIterator it = currentProgram.getListing().getInstructions(set, true);
        while (it.hasNext()) {
            Instruction ins = it.next();
            Symbol s = getSymbolAt(ins.getAddress());
            String lbl = (s != null) ? "  <" + s.getName() + ">" : "";
            out.println("//   " + ins.getAddress() + "  " + ins + lbl);
        }
        out.println();
    }
}
