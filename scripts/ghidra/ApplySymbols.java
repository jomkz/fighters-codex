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
import java.util.ArrayList;
import java.util.List;

public class ApplySymbols extends GhidraScript {

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args == null || args.length < 1 || args[0].isEmpty()) {
            println("ERROR: pass the repo root as the first script argument.");
            return;
        }
        File dbDir = new File(args[0], "db");
        File symbolsDir = new File(dbDir, "symbols");
        File[] files = symbolsDir.listFiles((d, n) -> n.endsWith(".csv"));
        if (files == null || files.length == 0) {
            println("ERROR: no CSV files under " + symbolsDir);
            return;
        }
        java.util.Arrays.sort(files);

        // Only apply this binary's symbol files — VAs collide across images
        // (IP.EXE bases at the same 0x00400000 as FA.EXE), so applying another
        // binary's rows here would stamp the wrong names.
        String binary = currentProgram.getDomainFile().getName();
        java.util.Set<String> binSlugs = ExportInventory.slugsForBinary(dbDir, binary);
        println("binary: " + binary + " (" + binSlugs.size() + " subsystem files)");

        SymbolTable st = currentProgram.getSymbolTable();
        int applied = 0, unchanged = 0, created = 0, conflicts = 0, waived = 0;
        // Func rows collected for the re-assertion pass below.
        List<Object[]> funcRows = new ArrayList<>();

        for (File f : files) {
            String slug = f.getName().replaceAll("\\.csv$", "");
            if (!binSlugs.contains(slug)) continue; // not this binary
            for (List<String> row : ExportInventory.readCsv(f)) {
                // va,kind,name,display,source,confidence,notes
                long va = Long.decode(row.get(0));
                String kind = row.get(1);
                String name = row.get(2);
                String source = row.get(4);
                if (source.equals("waiver")) { waived++; continue; }
                Address addr = toAddr(va);

                try {
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
                        funcRows.add(new Object[]{addr, name}); // for the re-assert pass
                        if (fn.getName().equals(name)) { unchanged++; continue; }
                        // The target name may already exist at this address as a
                        // non-primary label (e.g. from FA.SMS). Remove it so the
                        // function can adopt the name instead of aborting the run.
                        dropConflictingLabel(st, addr, name, fn.getSymbol());
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
                        // If a symbol with this name already exists here, make it
                        // primary rather than creating a duplicate.
                        Symbol existing = findSymbol(st, addr, name);
                        if (existing != null) {
                            existing.setPrimary();
                            applied++;
                            continue;
                        }
                        if (primary != null)
                            println(String.format("RELABEL 0x%08X: %s -> %s",
                                    va, primary.getName(), name));
                        Symbol s = st.createLabel(addr, name, SourceType.USER_DEFINED);
                        s.setPrimary();
                        applied++;
                    }
                } catch (Exception ex) {
                    println(String.format("CONFLICT 0x%08X (%s): %s",
                            va, name, ex.getMessage()));
                    conflicts++;
                }
            }
        }

        // Re-assertion pass. A thunk inherits its target's name, so a thunk whose
        // DB name is its target's OLD default (e.g. thunk_FUN_004d416b) matches by
        // inheritance in pass 1 and re-inherits when the target is renamed — the
        // 0x4D415D rebuild drift (#377). Now that every target is final, re-set any
        // func whose name drifted; the corrected name no longer equals the thunk
        // default, so it sticks. Deterministic regardless of row/analysis order.
        int reasserted = 0;
        for (Object[] fr : funcRows) {
            Address a = (Address) fr[0];
            String nm = (String) fr[1];
            Function fn = getFunctionAt(a);
            if (fn == null || fn.getName().equals(nm)) continue;
            try {
                dropConflictingLabel(st, a, nm, fn.getSymbol());
                println(String.format("RE-ASSERT 0x%08X: %s -> %s",
                        a.getOffset(), fn.getName(), nm));
                fn.setName(nm, SourceType.USER_DEFINED);
                reasserted++;
            } catch (Exception ex) {
                println(String.format("CONFLICT 0x%08X (%s): %s",
                        a.getOffset(), nm, ex.getMessage()));
                conflicts++;
            }
        }
        applied += reasserted;

        println(String.format(
                "ApplySymbols: %d applied (%d functions created, %d re-asserted), "
                + "%d unchanged, %d waived, %d conflicts",
                applied, created, reasserted, unchanged, waived, conflicts));
    }

    /** Symbol at addr with the given name, or null. */
    private Symbol findSymbol(SymbolTable st, Address addr, String name) {
        for (Symbol s : st.getSymbols(addr))
            if (s.getName().equals(name)) return s;
        return null;
    }

    /** Remove a label at addr that would collide with a rename to name (unless
     *  it is the function's own symbol). */
    private void dropConflictingLabel(SymbolTable st, Address addr, String name,
                                      Symbol keep) {
        Symbol s = findSymbol(st, addr, name);
        if (s != null && !s.equals(keep)) s.delete();
    }
}
