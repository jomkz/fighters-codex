// Export the Ghidra-project ground truth for the reconstruction program (#209/#247).
// Scoped to the current binary (this program's name, e.g. FA.EXE / WAIL32.DLL);
// image bounds come from the program, not a hardcoded window. Writes three
// byte-stable CSVs into <repo>/db/inventory/<binary>/:
//   functions.csv  va,name,size                 -- every function in the image
//   globals.csv    va,name,xref_count,subsystems -- data symbols with >=1 code xref
//   ranges.csv     slug,range,bytes,bytes_in_functions,functions -- per manifest range
//
// Subsystem tags come from <repo>/db/subsystems.csv (ranges) and
// <repo>/db/symbols/*.csv (explicit claims; a claim overrides range membership).
//
// Invoke: scripts/ghidra/export_inventory.sh  (passes the repo root as scriptArg)
// The output is committed; see db/README.md for the canonical-project rule.
//
// @category FightersAnthology
// @author fighters-toolkit

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.DataIterator;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceManager;
import ghidra.program.model.symbol.Symbol;

import java.io.BufferedWriter;
import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;
import java.util.TreeSet;

public class ExportInventory extends GhidraScript {

    // Image bounds are derived from the loaded program (getImageBase() + the
    // memory block extent), NOT hardcoded — FA.EXE is based at 0x00400000 but the
    // overlays sit elsewhere (WAIL32.DLL at 0x20000000, the comms DLLs at
    // 0x10000000), and IP.EXE collides with FA.EXE at 0x00400000.
    private long imageLo;
    private long imageHi;

    private static final class Subsystem {
        final String slug;
        final List<long[]> ranges = new ArrayList<>(); // {lo, hi} half-open
        Subsystem(String slug) { this.slug = slug; }
        boolean contains(long va) {
            for (long[] r : ranges)
                if (va >= r[0] && va < r[1]) return true;
            return false;
        }
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args == null || args.length < 1 || args[0].isEmpty()) {
            println("ERROR: pass the repo root as the first script argument.");
            return;
        }
        File repo = new File(args[0]);
        File dbDir = new File(repo, "db");
        if (!new File(dbDir, "subsystems.csv").isFile()) {
            println("ERROR: " + dbDir + "/subsystems.csv not found.");
            return;
        }

        // The current binary = this Ghidra program's name (imported filename),
        // e.g. FA.EXE / WAIL32.DLL. Everything is scoped to it.
        String binary = currentProgram.getDomainFile().getName();
        println("binary: " + binary);

        // Image bounds from the loaded program (base .. highest loaded block end).
        imageLo = currentProgram.getImageBase().getOffset();
        imageHi = imageLo;
        for (ghidra.program.model.mem.MemoryBlock b :
                currentProgram.getMemory().getBlocks()) {
            if (b.isLoaded() || b.isInitialized()) {
                imageLo = Math.min(imageLo, b.getStart().getOffset());
                imageHi = Math.max(imageHi, b.getEnd().getOffset() + 1);
            }
        }
        println(String.format("image bounds: 0x%08X .. 0x%08X", imageLo, imageHi));

        // Only this binary's subsystems + their claims count.
        java.util.Set<String> binSlugs = slugsForBinary(dbDir, binary);
        List<Subsystem> subsystems = readManifest(
                new File(dbDir, "subsystems.csv"), binSlugs);
        // Explicit claims: VA -> slug, from this binary's db/symbols/*.csv. A claim
        // overrides range membership for every other subsystem of the same binary.
        Map<Long, String> claims = readClaims(new File(dbDir, "symbols"), binSlugs);

        // Default output dir is per-binary: db/inventory/<binary>/. Optional 2nd arg
        // overrides it (used by the reproducibility audit to export elsewhere).
        File outDir = (args.length >= 2 && !args[1].isEmpty())
                ? new File(args[1]) : new File(new File(dbDir, "inventory"), binary);
        outDir.mkdirs();

        FunctionManager fm = currentProgram.getFunctionManager();
        ReferenceManager rm = currentProgram.getReferenceManager();

        // --- functions.csv ------------------------------------------------
        TreeMap<Long, Function> functions = new TreeMap<>();
        for (Function fn : fm.getFunctions(true)) {
            long va = fn.getEntryPoint().getOffset();
            if (va < imageLo || va >= imageHi || fn.isExternal()) continue;
            functions.put(va, fn);
        }
        try (BufferedWriter w = open(new File(outDir, "functions.csv"))) {
            w.write("va,name,size\n");
            for (Map.Entry<Long, Function> e : functions.entrySet()) {
                Function fn = e.getValue();
                w.write(String.format("0x%08X,%s,%d%n".replace("%n", "\n"),
                        e.getKey(), csv(fn.getName()),
                        fn.getBody().getNumAddresses()));
            }
        }
        println("functions.csv: " + functions.size() + " functions");

        // --- globals.csv ----------------------------------------------------
        // Data symbols (named or defined) with at least one xref from code.
        TreeMap<Long, String> dataNames = new TreeMap<>();
        for (Symbol sym : currentProgram.getSymbolTable().getAllSymbols(false)) {
            Address addr = sym.getAddress();
            long va = addr.getOffset();
            if (va < imageLo || va >= imageHi) continue;
            if (sym.isExternal() || fm.getFunctionAt(addr) != null) continue;
            if (!dataNames.containsKey(va)) dataNames.put(va, sym.getName());
        }
        DataIterator di = currentProgram.getListing()
                .getDefinedData(toAddr(imageLo), true);
        while (di.hasNext()) {
            Data d = di.next();
            long va = d.getAddress().getOffset();
            if (va >= imageHi) break;
            if (fm.getFunctionAt(d.getAddress()) != null) continue;
            dataNames.putIfAbsent(va, "<unnamed>");
        }
        int globalsOut = 0;
        try (BufferedWriter w = open(new File(outDir, "globals.csv"))) {
            w.write("va,name,xref_count,subsystems\n");
            for (Map.Entry<Long, String> e : dataNames.entrySet()) {
                long va = e.getKey();
                int xrefs = 0;
                TreeSet<String> tags = new TreeSet<>();
                for (Reference ref : rm.getReferencesTo(toAddr(va))) {
                    Function from = fm.getFunctionContaining(ref.getFromAddress());
                    if (from == null) continue; // only code xrefs count
                    xrefs++;
                    String tag = membership(from.getEntryPoint().getOffset(),
                                            subsystems, claims);
                    if (tag != null) tags.add(tag);
                }
                if (xrefs == 0) continue;
                w.write(String.format("0x%08X,%s,%d,%s\n",
                        va, csv(e.getValue()), xrefs, String.join(";", tags)));
                globalsOut++;
            }
        }
        println("globals.csv: " + globalsOut + " referenced data symbols");

        // --- ranges.csv -----------------------------------------------------
        try (BufferedWriter w = open(new File(outDir, "ranges.csv"))) {
            w.write("slug,range,bytes,bytes_in_functions,functions\n");
            for (Subsystem s : subsystems) {
                for (long[] r : s.ranges) {
                    AddressSet rangeSet =
                            new AddressSet(toAddr(r[0]), toAddr(r[1] - 1));
                    long covered = 0;
                    int count = 0;
                    for (Map.Entry<Long, Function> e : functions.entrySet()) {
                        Function fn = e.getValue();
                        covered += fn.getBody().intersect(rangeSet)
                                .getNumAddresses();
                        if (e.getKey() >= r[0] && e.getKey() < r[1]) count++;
                    }
                    w.write(String.format("%s,0x%06X-0x%06X,%d,%d,%d\n",
                            s.slug, r[0], r[1], r[1] - r[0], covered, count));
                }
            }
        }
        println("ranges.csv written");
    }

    /** Subsystem slug a function VA belongs to, or null. Claims win over ranges. */
    private String membership(long va, List<Subsystem> subsystems,
                              Map<Long, String> claims) {
        String claimed = claims.get(va);
        if (claimed != null) return claimed;
        for (Subsystem s : subsystems)
            if (s.contains(va)) return s.slug;
        return null;
    }

    /** Slugs in subsystems.csv whose `binary` column equals binaryName. */
    static java.util.Set<String> slugsForBinary(File dbDir, String binaryName)
            throws Exception {
        java.util.Set<String> out = new java.util.HashSet<>();
        for (List<String> row : readCsv(new File(dbDir, "subsystems.csv")))
            if (row.size() >= 3 && row.get(2).equals(binaryName)) out.add(row.get(0));
        return out;
    }

    private List<Subsystem> readManifest(File manifest, java.util.Set<String> binSlugs)
            throws Exception {
        List<Subsystem> out = new ArrayList<>();
        for (List<String> row : readCsv(manifest)) {
            if (!binSlugs.contains(row.get(0))) continue; // this binary only
            Subsystem s = new Subsystem(row.get(0));
            for (String r : row.get(3).split(";")) {
                String[] parts = r.split("-");
                s.ranges.add(new long[] {
                        Long.decode(parts[0]), Long.decode(parts[1]) });
            }
            out.add(s);
        }
        return out;
    }

    private Map<Long, String> readClaims(File symbolsDir, java.util.Set<String> binSlugs)
            throws Exception {
        Map<Long, String> out = new LinkedHashMap<>();
        File[] files = symbolsDir.listFiles((d, n) -> n.endsWith(".csv"));
        if (files == null) return out;
        for (File f : files) {
            String slug = f.getName().replaceAll("\\.csv$", "");
            if (!binSlugs.contains(slug)) continue; // this binary only
            for (List<String> row : readCsv(f))
                out.put(Long.decode(row.get(0)), slug);
        }
        return out;
    }

    /** Minimal CSV reader: header skipped, double-quoted fields supported. */
    static List<List<String>> readCsv(File f) throws Exception {
        List<List<String>> rows = new ArrayList<>();
        List<String> lines = Files.readAllLines(f.toPath(), StandardCharsets.UTF_8);
        for (int i = 1; i < lines.size(); i++) {
            String line = lines.get(i);
            if (line.isEmpty()) continue;
            List<String> fields = new ArrayList<>();
            StringBuilder cur = new StringBuilder();
            boolean quoted = false;
            for (int j = 0; j < line.length(); j++) {
                char c = line.charAt(j);
                if (quoted) {
                    if (c == '"') quoted = false;
                    else cur.append(c);
                } else if (c == '"') {
                    quoted = true;
                } else if (c == ',') {
                    fields.add(cur.toString());
                    cur.setLength(0);
                } else {
                    cur.append(c);
                }
            }
            fields.add(cur.toString());
            rows.add(fields);
        }
        return rows;
    }

    private static BufferedWriter open(File f) throws Exception {
        return Files.newBufferedWriter(f.toPath(), StandardCharsets.UTF_8);
    }

    private static String csv(String s) {
        return s.replace(",", ";");
    }
}
