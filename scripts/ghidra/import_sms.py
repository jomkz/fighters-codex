# import_sms.py — Ghidra Jython script
#
# Bulk-imports all FA.SMS symbols into the current Ghidra program as named labels.
# Run from: Tools → Script Manager → import_sms.py
#
# Tested against Ghidra 11.x. Requires the current program to be FA.EXE or an
# overlay DLL rebased to FA.EXE's preferred base (0x00400000).
#
# @category FightersAnthology
# @author fighters-toolkit

import struct
from ghidra.program.model.symbol import SourceType

sms_file = askFile("Select FA.SMS", "Open")
data = open(sms_file.getAbsolutePath(), "rb").read()

if len(data) < 4:
    popup("FA.SMS is too small to be valid.")
    raise Exception("invalid FA.SMS")

count = struct.unpack_from("<I", data, 0)[0]
strtab_off = 4 + count * 8

if strtab_off > len(data):
    popup("FA.SMS record table overruns the file.")
    raise Exception("invalid FA.SMS")

sym_table = currentProgram.getSymbolTable()
monitor.setMaximum(count)
monitor.setMessage("Importing FA.SMS symbols...")

imported = 0
skipped = 0

for i in range(count):
    if monitor.isCancelled():
        break

    rec_off = 4 + i * 8
    str_off, va = struct.unpack_from("<II", data, rec_off)

    abs_str_off = strtab_off + str_off
    if abs_str_off >= len(data):
        skipped += 1
        continue

    null = data.find(b"\x00", abs_str_off)
    if null < 0:
        null = len(data)
    name = data[abs_str_off:null].decode("ascii", errors="replace")
    if not name:
        skipped += 1
        continue

    addr = toAddr(va)
    sym_table.createLabel(addr, name, SourceType.IMPORTED)
    imported += 1
    monitor.incrementProgress(1)

println("Done: %d symbols imported, %d skipped." % (imported, skipped))
