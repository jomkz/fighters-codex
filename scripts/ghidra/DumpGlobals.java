// Global variable inventory for FA.EXE.
// Enumerates all defined data symbols (non-function), writing two CSVs with:
//   address, name, size_bytes, data_type, xref_count, first_writer
//   - DumpGlobals.csv        every data symbol (incl. Ghidra switch/case and
//                            unnamed data) — the raw full export.
//   - DumpGlobals_named.csv  only globals carrying a real assigned name, i.e.
//                            SourceType USER_DEFINED (db/ApplySymbols) or
//                            IMPORTED (FA.SMS import) — Ghidra's auto-analysis
//                            labels (switchD_/caseD_/s_/DAT_) are excluded.
//                            This is the listing docs/fa/globals.md cites.
// Invoke: run_ghidra.sh DumpGlobals.java
// Output: $FA_PROJECT/output/DumpGlobals.csv, $FA_PROJECT/output/DumpGlobals_named.csv

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.data.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class DumpGlobals extends GhidraScript {

    @Override
    public void run() throws Exception {
        String projectDir = System.getenv("FA_PROJECT");
        if (projectDir == null || projectDir.isEmpty())
            projectDir = System.getProperty("java.io.tmpdir");
        File outDir = new File(projectDir, "output");
        outDir.mkdirs();
        File outFile = new File(outDir, "DumpGlobals.csv");
        File namedFile = new File(outDir, "DumpGlobals_named.csv");

        String header = "address,name,size_bytes,data_type,xref_count,first_writer";
        PrintWriter out = new PrintWriter(new FileWriter(outFile));
        PrintWriter named = new PrintWriter(new FileWriter(namedFile));
        out.println(header);
        named.println(header);
        int namedCount = 0;

        FunctionManager fm = currentProgram.getFunctionManager();
        ReferenceManager rm = currentProgram.getReferenceManager();
        Listing listing = currentProgram.getListing();
        SymbolTable st = currentProgram.getSymbolTable();

        // Scan the FA.EXE address range: code + data segments
        Address lo = toAddr(0x00400000L);
        Address hi = toAddr(0x00600000L);

        Set<Long> seen = new LinkedHashSet<>();

        // Walk all symbols
        for (Symbol sym : st.getAllSymbols(false)) {
            Address addr = sym.getAddress();
            long va = addr.getOffset();
            if (va < 0x00400000L || va > 0x00600000L) continue;
            if (seen.contains(va)) continue;

            // Skip function entry points
            if (fm.getFunctionAt(addr) != null) continue;

            // Skip dynamic storage (external/library symbols)
            if (sym.isExternal()) continue;

            // Determine data item
            Data data = listing.getDefinedDataAt(addr);
            String typeName = "undefined";
            int size = 0;
            if (data != null) {
                DataType dt = data.getDataType();
                typeName = dt.getDisplayName().replace(",", ";");
                size = data.getLength();
            } else {
                // Check for undefined byte
                Data undef = listing.getDataAt(addr);
                if (undef != null) {
                    size = undef.getLength();
                    typeName = "undefined";
                }
            }

            // Count xrefs
            int xrefCount = 0;
            String firstWriter = "";
            for (Reference ref : rm.getReferencesTo(addr)) {
                xrefCount++;
                if (firstWriter.isEmpty() && ref.getReferenceType().isWrite()) {
                    Function fn = fm.getFunctionContaining(ref.getFromAddress());
                    if (fn != null) firstWriter = fn.getName();
                }
            }

            String name = sym.getName().replace(",", ";");
            String row = "0x" + Long.toHexString(va).toUpperCase()
                    + "," + name
                    + "," + size
                    + "," + typeName
                    + "," + xrefCount
                    + "," + firstWriter;
            out.println(row);
            // Named listing: only globals we (or FA.SMS) assigned a name to,
            // not Ghidra's auto-analysis labels (switchD_/caseD_/s_/DAT_).
            SourceType src = sym.getSource();
            if (src == SourceType.USER_DEFINED || src == SourceType.IMPORTED) {
                named.println(row);
                namedCount++;
            }
            seen.add(va);
        }

        // Second pass: walk defined data that has no named symbol
        DataIterator di = listing.getDefinedData(lo, true);
        while (di.hasNext()) {
            Data data = di.next();
            Address addr = data.getAddress();
            if (addr.compareTo(hi) > 0) break;
            long va = addr.getOffset();
            if (seen.contains(va)) continue;
            if (fm.getFunctionAt(addr) != null) continue;

            DataType dt = data.getDataType();
            String typeName = dt.getDisplayName().replace(",", ";");
            int size = data.getLength();

            int xrefCount = 0;
            String firstWriter = "";
            for (Reference ref : rm.getReferencesTo(addr)) {
                xrefCount++;
                if (firstWriter.isEmpty() && ref.getReferenceType().isWrite()) {
                    Function fn = fm.getFunctionContaining(ref.getFromAddress());
                    if (fn != null) firstWriter = fn.getName();
                }
            }

            out.println("0x" + Long.toHexString(va).toUpperCase()
                    + ",<unnamed>"
                    + "," + size
                    + "," + typeName
                    + "," + xrefCount
                    + "," + firstWriter);
            seen.add(va);
        }

        out.close();
        named.close();
        println("DumpGlobals complete: " + seen.size() + " entries -> " + outFile.getAbsolutePath());
        println("DumpGlobals_named complete: " + namedCount + " named globals -> " + namedFile.getAbsolutePath());
    }
}
