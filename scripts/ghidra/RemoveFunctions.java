// Remove default-named (FUN_*) functions listed in a file, one hex VA per line.
//
// The escape hatch for a bad sweep: SweepUndefinedCode created functions from what turned
// out to be MSVC's inter-function NOP fill (`mov edi,edi; lea esp,[esp+0]`), and a wrong
// function is worse than an undefined gap -- it inflates the denominator with fiction.
// Only FUN_* entries are removed; a named function is never touched.
//
// Invoke: run_ghidra.sh RemoveFunctions.java <file-of-VAs>
//
// @category FightersAnthology
// @author fighters-toolkit

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

import java.io.File;
import java.nio.file.Files;

public class RemoveFunctions extends GhidraScript {

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args == null || args.length < 1) {
            println("ERROR: pass a file of hex VAs (one per line).");
            return;
        }
        int removed = 0, kept = 0, absent = 0;
        for (String line : Files.readAllLines(new File(args[0]).toPath())) {
            line = line.trim();
            if (line.isEmpty()) continue;
            Address a = toAddr(Long.parseLong(line.replace("0x", ""), 16));
            Function f = getFunctionAt(a);
            if (f == null) { absent++; continue; }
            if (!f.getName().startsWith("FUN_")) { kept++; continue; }
            removeFunctionAt(a);
            removed++;
        }
        println(String.format("removed %d FUN_ functions, kept %d named, %d absent",
                removed, kept, absent));
    }
}
