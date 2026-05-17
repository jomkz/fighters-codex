// Bulk-imports all FA.SMS symbols into the current Ghidra program as named labels.
// Run from: Tools → Script Manager → ImportFASms.java
//
// The current program should be FA.EXE or an overlay DLL rebased to FA.EXE's
// preferred base (0x00400000). See scripts/README.md for overlay DLL instructions.
//
// @category FightersAnthology
// @author fighters-toolkit

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.symbol.SourceType;

import java.io.File;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

public class ImportFASms extends GhidraScript {

    @Override
    public void run() throws Exception {
        File smsFile = askFile("Select FA.SMS", "Open");
        byte[] data = Files.readAllBytes(smsFile.toPath());

        if (data.length < 4) {
            popup("FA.SMS is too small to be valid.");
            return;
        }

        ByteBuffer buf = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN);
        int count = buf.getInt(0);
        int strtabOff = 4 + count * 8;

        if (strtabOff > data.length) {
            popup("FA.SMS record table overruns the file.");
            return;
        }

        monitor.setMaximum(count);
        monitor.setMessage("Importing FA.SMS symbols...");

        var symTable = currentProgram.getSymbolTable();
        int imported = 0;
        int skipped = 0;

        for (int i = 0; i < count; i++) {
            if (monitor.isCancelled()) break;

            int recOff = 4 + i * 8;
            int strOff = buf.getInt(recOff);
            long va    = Integer.toUnsignedLong(buf.getInt(recOff + 4));

            int absStrOff = strtabOff + strOff;
            if (absStrOff >= data.length) {
                skipped++;
                continue;
            }

            int end = absStrOff;
            while (end < data.length && data[end] != 0) end++;

            String name = new String(data, absStrOff, end - absStrOff, StandardCharsets.US_ASCII);
            if (name.isEmpty()) {
                skipped++;
                continue;
            }

            Address addr = toAddr(va);
            symTable.createLabel(addr, name, SourceType.IMPORTED);
            imported++;
            monitor.incrementProgress(1);
        }

        println(String.format("Done: %d symbols imported, %d skipped.", imported, skipped));
    }
}
