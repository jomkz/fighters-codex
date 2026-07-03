// SH header field consumption analysis (issue #124).
//
// The shape code section starts [FF FF][unk0 i16][unk1 i16][scale i16][ext i16[3]];
// GRAddBrentObj (0x4d057c) provably reads +4 (unk1) and +6 (scale) and starts the
// instruction stream at +0xe. This script closes the remaining gap for unk0 (+2):
// the hand-written interpreter around the do_* labels (0x4cd000-0x4d7000) contains
// code Ghidra may not have attached to functions, so scan the region's raw listing
// for any 16-bit access with a header-plausible displacement.

import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.address.AddressSetView;
import ghidra.program.model.address.AddressRange;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;

public class AnalyzeSHHeader extends FAScript {

    private static final long START = 0x4cd000L;
    private static final long END = 0x4d7000L;

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeSHHeader");
        Address start = toAddr(START);
        Address end = toAddr(END - 1);
        AddressSet region = new AddressSet(start, end);

        header("Undefined ranges in interpreter region 0x4cd000-0x4d7000");
        AddressSetView undef = currentProgram.getListing()
                .getUndefinedRanges(region, false, monitor);
        long undefBytes = 0;
        for (AddressRange r : undef) {
            out.println("//   " + r.getMinAddress() + " - " + r.getMaxAddress()
                    + "  (" + r.getLength() + " bytes)");
            undefBytes += r.getLength();
        }
        out.println("// total undefined bytes: " + undefBytes);

        header("Instructions outside any function in region");
        int gaps = 0;
        InstructionIterator it = currentProgram.getListing().getInstructions(region, true);
        while (it.hasNext()) {
            Instruction ins = it.next();
            Function fn = currentProgram.getFunctionManager()
                    .getFunctionContaining(ins.getAddress());
            if (fn == null) {
                out.println("//   " + ins.getAddress() + "  " + ins);
                gaps++;
            }
        }
        out.println("// instructions outside functions: " + gaps);

        header("16-bit accesses with header-plausible displacement (+0x2/-0x8/-0xa/-0xc) in region");
        it = currentProgram.getListing().getInstructions(region, true);
        while (it.hasNext()) {
            Instruction ins = it.next();
            String s = ins.toString();
            if (s.matches(".*\\[[A-Z]{3} \\+ 0x2\\].*")
                    || s.matches(".*\\[[A-Z]{3} + \\-0x[8ac]\\].*")
                    || s.matches(".*\\[[A-Z]{3} \\- 0x[8ac]\\].*")) {
                Function fn = currentProgram.getFunctionManager()
                        .getFunctionContaining(ins.getAddress());
                out.println("//   " + ins.getAddress() + "  " + s
                        + (fn != null ? "   (in " + fn.getName() + ")" : "   (GAP)"));
            }
        }

        header("Raw listing around do_start_interp (0x4d4230-0x4d4290)");
        it = currentProgram.getListing()
                .getInstructions(new AddressSet(toAddr(0x4d4230L), toAddr(0x4d428fL)), true);
        while (it.hasNext()) {
            Instruction ins = it.next();
            out.println("//   " + ins.getAddress() + "  " + ins);
        }

        closeOutput();
    }
}
