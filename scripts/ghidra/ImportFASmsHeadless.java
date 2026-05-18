// Headless-compatible FA.SMS symbol importer.
// Reads the FA.SMS path from the first script argument (-scriptArg).
// Falls back to %FA_PROJECT%\FA.SMS, then %USERPROFILE%\src\fa\FA.SMS.
//
// Run from: run_ghidra.bat ImportFASmsHeadless.java
//
// The current program should be FA.EXE or an overlay DLL rebased to FA.EXE's
// preferred base (0x00400000). See scripts/ghidra/README.md for overlay setup.
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

public class ImportFASmsHeadless extends GhidraScript {

    @Override
    public void run() throws Exception {
        File smsFile = resolveSmsFile();
        if (smsFile == null || !smsFile.exists()) {
            println("FA.SMS not found. Pass the path via -scriptArg, or set FA_PROJECT.");
            return;
        }
        println("Loading FA.SMS from: " + smsFile.getAbsolutePath());

        byte[] data = Files.readAllBytes(smsFile.toPath());
        if (data.length < 4) {
            println("FA.SMS is too small to be valid.");
            return;
        }

        ByteBuffer buf = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN);
        int count = buf.getInt(0);
        int strtabOff = 4 + count * 8;

        if (strtabOff > data.length) {
            println("FA.SMS record table overruns the file.");
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
            if (absStrOff >= data.length) { skipped++; continue; }

            int end = absStrOff;
            while (end < data.length && data[end] != 0) end++;

            String name = new String(data, absStrOff, end - absStrOff, StandardCharsets.US_ASCII);
            if (name.isEmpty()) { skipped++; continue; }

            Address addr = toAddr(va);
            symTable.createLabel(addr, name, SourceType.IMPORTED);
            imported++;
            monitor.incrementProgress(1);
        }

        println(String.format("Done: %d symbols imported, %d skipped.", imported, skipped));
    }

    private File resolveSmsFile() {
        // 1. Explicit -scriptArg path
        String[] args = getScriptArgs();
        if (args != null && args.length > 0 && !args[0].isEmpty())
            return new File(args[0]);

        // 2. %FA_PROJECT%\FA.SMS
        String proj = System.getenv("FA_PROJECT");
        if (proj != null && !proj.isEmpty()) {
            File f = new File(proj, "FA.SMS");
            if (f.exists()) return f;
        }

        // 3. Default FA install path
        return new File("C:\\JANES\\Fighters Anthology\\FA.SMS");
    }
}
