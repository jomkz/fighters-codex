// Apply the committed symbol database (db/symbols/*.csv) to the Ghidra project.
// The DB is canonical: rows are applied as USER_DEFINED names; a differing
// existing name is overwritten (and logged). Idempotent — a second run reports
// zero changes. Waiver rows are skipped.
//
//   func rows: ensure a function exists at the VA (disassemble + create if
//              needed), then set its name.
//   data rows: set the primary label at the VA.
//
// Invoke: scripts/ghidra/apply_symbols.sh  (passes the repo root as scriptArg)
// See db/README.md for the schema and workflow.
//
// @category FightersAnthology
// @author fighters-toolkit

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolTable;

import java.io.File;
import java.util.List;

public class ApplySymbols extends GhidraScript {

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args == null || args.length < 1 || args[0].isEmpty()) {
            println("ERROR: pass the repo root as the first script argument.");
            return;
        }
        File symbolsDir = new File(new File(args[0], "db"), "symbols");
        File[] files = symbolsDir.listFiles((d, n) -> n.endsWith(".csv"));
        if (files == null || files.length == 0) {
            println("ERROR: no CSV files under " + symbolsDir);
            return;
        }
        java.util.Arrays.sort(files);

        SymbolTable st = currentProgram.getSymbolTable();
        int applied = 0, unchanged = 0, created = 0, conflicts = 0, waived = 0;

        for (File f : files) {
            for (List<String> row : ExportInventory.readCsv(f)) {
                // va,kind,name,display,source,confidence,notes
                long va = Long.decode(row.get(0));
                String kind = row.get(1);
                String name = row.get(2);
                String source = row.get(4);
                if (source.equals("waiver")) { waived++; continue; }
                Address addr = toAddr(va);

                if (kind.equals("func")) {
                    Function fn = getFunctionAt(addr);
                    if (fn == null) {
                        if (getInstructionAt(addr) == null) disassemble(addr);
                        fn = createFunction(addr, name);
                        if (fn == null) {
                            println(String.format(
                                    "CONFLICT 0x%08X: cannot create function", va));
                            conflicts++;
                            continue;
                        }
                        created++;
                    }
                    if (fn.getName().equals(name)) { unchanged++; continue; }
                    println(String.format("RENAME 0x%08X: %s -> %s",
                            va, fn.getName(), name));
                    fn.setName(name, SourceType.USER_DEFINED);
                    applied++;
                } else {
                    Symbol primary = st.getPrimarySymbol(addr);
                    if (primary != null && primary.getName().equals(name)) {
                        unchanged++;
                        continue;
                    }
                    if (primary != null)
                        println(String.format("RELABEL 0x%08X: %s -> %s",
                                va, primary.getName(), name));
                    Symbol s = st.createLabel(addr, name, SourceType.USER_DEFINED);
                    s.setPrimary();
                    applied++;
                }
            }
        }
        println(String.format(
                "ApplySymbols: %d applied (%d functions created), %d unchanged, "
                + "%d waived, %d conflicts",
                applied, created, unchanged, waived, conflicts));
    }
}
