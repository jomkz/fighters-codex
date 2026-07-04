// Apply recovered datatypes to the Ghidra project (reconstruction typing pass, #230).
// Two steps:
//   1) parse every db/types/*.h into the program's DataTypeManager (struct layouts +
//      the type vocabulary);
//   2) for each db/symbols/*.csv row with a non-empty `type` column, resolve that C
//      type and apply it — a datatype at a data symbol, or (if given) a function
//      signature. Waiver rows and empty types are skipped.
//
// The DB is canonical and this is idempotent: a second run reports zero changes.
// Per-row failures are logged and skipped so one bad type never aborts the run.
//
// Invoke: scripts/ghidra/apply_types.sh  (passes the repo root as scriptArg; run
// apply_symbols.sh first so the names exist). See db/types/README.md.
//
// @category FightersAnthology
// @author fighters-toolkit

import ghidra.app.cmd.function.ApplyFunctionSignatureCmd;
import ghidra.app.script.GhidraScript;
import ghidra.app.util.cparser.C.CParser;
import ghidra.app.util.parser.FunctionSignatureParser;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.DataType;
import ghidra.program.model.data.DataTypeManager;
import ghidra.program.model.data.FunctionDefinitionDataType;
import ghidra.program.model.data.DataUtilities;
import ghidra.program.model.data.DataUtilities.ClearDataMode;
import ghidra.program.model.listing.Function;
import ghidra.util.data.DataTypeParser;
import ghidra.util.data.DataTypeParser.AllowedDataTypes;

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.List;

public class ApplyTypes extends GhidraScript {

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args == null || args.length < 1 || args[0].isEmpty()) {
            println("ERROR: pass the repo root as the first script argument.");
            return;
        }
        File repo = new File(args[0]);
        File dbDir = new File(repo, "db");
        DataTypeManager dtm = currentProgram.getDataTypeManager();

        // Scope to the current binary (this program's name).
        String binary = currentProgram.getDomainFile().getName();
        java.util.Set<String> binSlugs = ExportInventory.slugsForBinary(dbDir, binary);
        println("binary: " + binary);

        // --- 1. parse db/types/*.h into the program DTM -----------------------
        // Top-level headers are shared; per-binary struct headers live in
        // db/types/<binary>/ and are parsed only for their program.
        File typesDir = new File(dbDir, "types");
        java.util.List<File> headerList = new java.util.ArrayList<>();
        File[] shared = typesDir.listFiles((d, n) -> n.endsWith(".h"));
        if (shared != null) headerList.addAll(java.util.Arrays.asList(shared));
        File[] perBin = new File(typesDir, binary).listFiles((d, n) -> n.endsWith(".h"));
        if (perBin != null) headerList.addAll(java.util.Arrays.asList(perBin));
        File[] headers = headerList.toArray(new File[0]);
        int parsed = 0;
        if (headers != null) {
            java.util.Arrays.sort(headers);
            for (File h : headers) {
                String text = new String(Files.readAllBytes(h.toPath()),
                        StandardCharsets.UTF_8);
                try {
                    CParser parser = new CParser(dtm, true, new DataTypeManager[0]);
                    parser.parse(text);
                    parsed++;
                    println("parsed " + h.getName());
                } catch (Exception ex) {
                    println("ERROR parsing " + h.getName() + ": " + ex.getMessage());
                }
            }
        }
        println("headers parsed: " + parsed);

        // --- 2. apply the `type` column (this binary's symbol files only) -----
        File symbolsDir = new File(dbDir, "symbols");
        File[] files = symbolsDir.listFiles((d, n) -> n.endsWith(".csv"));
        if (files == null) { println("no symbol CSVs"); return; }
        java.util.Arrays.sort(files);

        DataTypeParser dtp = new DataTypeParser(dtm, dtm, null, AllowedDataTypes.ALL);
        FunctionSignatureParser sigp = new FunctionSignatureParser(dtm, null);
        int dataApplied = 0, funcApplied = 0, unchanged = 0, skipped = 0, failed = 0;

        for (File f : files) {
            String slug = f.getName().replaceAll("\\.csv$", "");
            if (!binSlugs.contains(slug)) continue; // not this binary
            for (List<String> row : ExportInventory.readCsv(f)) {
                // va,kind,name,display,source,confidence,notes,type
                if (row.size() < 8) continue;
                String kind = row.get(1);
                String source = row.get(4);
                String ctype = row.get(7).trim();
                if (source.equals("waiver") || ctype.isEmpty()) { skipped++; continue; }
                long va = Long.decode(row.get(0));
                Address addr = toAddr(va);
                try {
                    if (kind.equals("data")) {
                        DataType dt = dtp.parse(ctype);
                        if (dt == null) { failed++; continue; }
                        DataUtilities.createData(currentProgram, addr, dt, dt.getLength(),
                                false, ClearDataMode.CLEAR_ALL_CONFLICT_DATA);
                        dataApplied++;
                    } else { // func: treat `type` as a full signature override
                        Function fn = getFunctionAt(addr);
                        if (fn == null) { skipped++; continue; }
                        FunctionDefinitionDataType sig = sigp.parse(null, ctype);
                        if (sig == null) { failed++; continue; }
                        ApplyFunctionSignatureCmd cmd = new ApplyFunctionSignatureCmd(
                                addr, sig, ghidra.program.model.symbol.SourceType.USER_DEFINED);
                        if (cmd.applyTo(currentProgram)) funcApplied++;
                        else failed++;
                    }
                } catch (Exception ex) {
                    println(String.format("TYPE FAIL 0x%08X (%s): %s",
                            va, ctype, ex.getMessage()));
                    failed++;
                }
            }
        }
        println(String.format(
                "ApplyTypes: %d data typed, %d funcs signed, %d skipped, %d failed",
                dataApplied, funcApplied, skipped, failed));
    }
}
