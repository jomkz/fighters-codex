// CB8 player trace (#95): find the DRBC magic reference in FA.EXE, decompile
// the open/validate function, its callers, and two levels of callees — the
// MRFI frame-decode loop and the palette handling live in that neighborhood.
// The MRFI/MRFA/VooM tags do not appear as bytes in the image, so the player
// evidently trusts the index rather than checking chunk tags.

import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import java.util.LinkedHashSet;
import java.util.Set;

public class AnalyzeCB8 extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeCB8");

        header("CB8 -- magic byte sites (string or immediate)");
        searchStrings(new String[]{"DRBC", "MRFI", "MRFA", "VooM", ".CB8"});

        Address a = currentProgram.getMemory().findBytes(
                currentProgram.getMinAddress(), "DRBC".getBytes(), null, true, monitor);
        while (a != null) {
            header("CB8 -- DRBC bytes @ " + a);
            Set<Function> roots = new LinkedHashSet<>();
            // Data refs to the site (a string datum)...
            for (Reference ref : currentProgram.getReferenceManager().getReferencesTo(a)) {
                Function fn = currentProgram.getFunctionManager()
                        .getFunctionContaining(ref.getFromAddress());
                if (fn != null) roots.add(fn);
            }
            // ...or the bytes are an immediate operand inside an instruction:
            // the containing function IS the magic check.
            Function holder = currentProgram.getFunctionManager().getFunctionContaining(a);
            if (holder != null) roots.add(holder);

            for (Function fn : roots) {
                header("CB8 -- root " + fn.getName() + " @ " + fn.getEntryPoint());
                dumpAt(fn.getEntryPoint().getOffset());
                dumpCallers(fn.getEntryPoint().getOffset());
                for (Function c1 : fn.getCalledFunctions(monitor)) {
                    dumpAt(c1.getEntryPoint().getOffset());
                    for (Function c2 : c1.getCalledFunctions(monitor)) {
                        dumpAt(c2.getEntryPoint().getOffset());
                    }
                }
            }
            a = currentProgram.getMemory().findBytes(
                    a.add(1), "DRBC".getBytes(), null, true, monitor);
        }

        // Phase 2 — the frame path. InitCobra proves Cobra == the CB8 player,
        // so the video-decode cluster is CB8's frame codec. Dump the play
        // loop, the frame dispatcher (callers of the SVGA8 decoders), and the
        // codebook machinery.
        header("CB8 -- PlayCobra and the .CB8 reference holders");
        dumpAt(0x421a50);   // _PlayCobra@4
        dumpAt(0x4613b0);   // FUN_004613b0 (.CB8 string ref)
        header("CB8 -- InitVideo / frame dispatch");
        dumpAt(0x46b4e0);   // SetupCobra (context)
        dumpSymbolsMatching("InitVideo", "DecodeFrame", "CobraFrame", "NextFrame");
        header("CB8 -- SVGA8 decoders + callers (the dispatcher)");
        dumpAt(0x456EC0);   // DecodeSVGA8Frame
        dumpCallers(0x456EC0);
        dumpAt(0x456300);   // DecodeDSVGA8Frame
        dumpCallers(0x456300);
        header("CB8 -- codebook machinery");
        dumpAt(0x456AD0);   // EDB (expand-book)
        dumpCallers(0x456AD0);
        dumpAt(0x457230);   // DecodeDBook
        dumpCallers(0x457230);

        // Phase 3 — the pixel kernels DecodeSVGA8Frame calls: exact 4x4 and
        // 2x2 placement, plus the non-dither ExpandDB variant.
        header("CB8 -- pixel kernels");
        dumpSymbolsMatching("copysb8", "copydb8", "expanddb", "copydsb8", "copyddb8");

        closeOutput();
    }
}
