// One-off investigation (#452): for each VA in a file, report what actually references it
// and what named symbol it sits behind, so the newly-surfaced globals can be dispositioned
// on evidence instead of on a guess about their stride.
//
// Invoke: scripts/ghidra/run_ghidra.sh InspectGlobals.java /path/to/vas.txt
//@category FA

import java.io.BufferedReader;
import java.io.FileReader;
import java.util.ArrayList;
import java.util.List;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

public class InspectGlobals extends GhidraScript {

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) { println("usage: InspectGlobals <va-file>"); return; }

        List<Long> vas = new ArrayList<>();
        try (BufferedReader br = new BufferedReader(new FileReader(args[0]))) {
            String line;
            while ((line = br.readLine()) != null) {
                line = line.trim();
                if (!line.isEmpty()) vas.add(Long.decode(line));
            }
        }

        for (long va : vas) {
            Address addr = toAddr(va);

            // Which functions reach this address, and how?
            StringBuilder refs = new StringBuilder();
            ReferenceIterator ri = currentProgram.getReferenceManager().getReferencesTo(addr);
            int n = 0;
            while (ri.hasNext() && n < 4) {
                Reference r = ri.next();
                Function f = getFunctionContaining(r.getFromAddress());
                refs.append(f == null ? "?" : f.getName())
                    .append("[").append(r.getReferenceType()).append("] ");
                n++;
            }

            // The nearest named symbol at or below the address: what block is it inside?
            Address probe = addr;
            String base = "?";
            long dist = -1;
            for (int back = 0; back < 0x400 && probe != null; back++) {
                Symbol[] syms = currentProgram.getSymbolTable().getSymbols(probe);
                if (syms != null && syms.length > 0 && !syms[0].getName().startsWith("DAT_")) {
                    base = syms[0].getName();
                    dist = va - probe.getOffset();
                    break;
                }
                probe = probe.subtract(1);
            }

            ghidra.program.model.listing.Data d = getDataAt(addr);
            println(String.format("0x%08X  type=%-12s base=%s+0x%X  refs: %s",
                    va, d == null ? "(none)" : d.getDataType().getName(), base, dist,
                    refs.toString().trim()));
        }
    }
}
