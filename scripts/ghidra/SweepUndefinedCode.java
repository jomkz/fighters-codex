// Disassemble the executable bytes that lie in no function body (#496).
//
// Ghidra's auto-analysis defines a function where it sees a CALL. Code reached only
// through a function POINTER -- window procs, thread entries, object proc-table entries,
// event callbacks -- is therefore never disassembled. Where such code carries an FA.SMS
// name, a db/symbols row materialises it (ApplySymbols disassembles + creates it) and no
// sweep is needed. What is left is the code the binary does NOT name.
//
// This script sweeps that remainder -- but only where there is EVIDENCE the bytes are a
// function entry. Blindly disassembling every gap manufactures junk functions out of
// jump tables and string pools, and a junk function is worse than an honest gap: it
// inflates the denominator with fiction and then reports coverage against it.
//
// Evidence, in order:
//   REF       -- a CALL to the address, or a DATA reference (something stores the address --
//                which is exactly how a proc-table entry or callback is reached).
//   PROLOGUE  -- the bytes open with an x86 function prologue this build actually emits.
//
// A JUMP to the address is deliberately NOT evidence: its target is as likely a switch case
// or loop head inside an existing function, and promoting one manufactures a fragment.
// A run with neither signal is LEFT ALONE and reported, not guessed at.
//
// Invoke: run_ghidra.sh SweepUndefinedCode.java [--write]   (default: dry run, no changes)
//
// @category FightersAnthology
// @author fighters-toolkit

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.symbol.FlowType;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.RefType;
import ghidra.program.model.symbol.Reference;

import java.util.ArrayList;
import java.util.List;

public class SweepUndefinedCode extends GhidraScript {

    private boolean write;
    private int refs, prologues, skipped, created, failed, tables, rejected;
    private long sweptBytes, skippedBytes;

    @Override
    public void run() throws Exception {
        for (String a : getScriptArgs())
            if ("--write".equals(a)) write = true;
        println(write ? "MODE: write (functions will be created)" : "MODE: dry run");

        AddressSet bodies = new AddressSet();
        for (Function f : currentProgram.getFunctionManager().getFunctions(true))
            bodies.add(f.getBody());

        // Collect the runs first: creating functions mutates the body set as we go.
        List<long[]> runs = new ArrayList<>();   // {start, end}
        for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
            if (!b.isExecute() || !b.isInitialized()) continue;
            long lo = b.getStart().getOffset(), hi = b.getEnd().getOffset() + 1;
            for (long va = lo; va < hi; ) {
                if (bodies.contains(toAddr(va))) { va++; continue; }
                long start = va;
                while (va < hi && !bodies.contains(toAddr(va))) va++;
                runs.add(new long[] {start, va});
            }
        }
        println("undefined runs: " + runs.size());

        for (long[] r : runs) {
            if (monitor.isCancelled()) break;
            long start = skipFill(r[0], r[1]);
            if (start < 0) continue;                       // pure alignment fill
            long len = r[1] - start;
            if (len < 8) continue;                         // slivers: not a function

            if (looksLikePointerTable(start)) { tables++; skippedBytes += len; continue; }

            String why = evidence(start);
            if (why == null) { skipped++; skippedBytes += len; continue; }

            if ("REF".equals(why)) refs++;
            else prologues++;
            sweptBytes += len;
            println(String.format("CAND 0x%08X %6d %s", start, len, why));

            if (!write) continue;
            Address a = toAddr(start);
            if (getInstructionAt(a) == null) disassemble(a);
            Function f = createFunction(a, null);          // null => Ghidra's FUN_ default name
            if (f == null) { failed++; continue; }
            if (!returns(f)) {                             // decoded garbage, not a function
                removeFunctionAt(a);
                rejected++;
                continue;
            }
            created++;
        }

        println(String.format("evidence: %d REF, %d PROLOGUE  (%d bytes)",
                refs, prologues, sweptBytes));
        println(String.format("no evidence -- left alone: %d runs, %d pointer tables (%d bytes)",
                skipped, tables, skippedBytes));
        if (write)
            println(String.format("created %d functions, %d rejected (no terminator), %d failed",
                    created, rejected, failed));
    }

    /** First non-fill byte of [lo,hi), or -1 if the run is all alignment fill.
     *
     *  MSVC does not pad with 0x90 alone: it aligns the NEXT function with multi-byte NOPs
     *  -- `mov edi,edi` (8B FF), `lea esp,[esp+0]` (8D A4 24 00 00 00 00), `lea ecx,[ecx+0]`
     *  (8D 49 00), `xchg ax,ax` (66 90). Reading 8B FF as a hot-patch PROLOGUE (which is what
     *  it is in later toolchains) made the first sweep create ~90 nine-byte "functions" out of
     *  the fill itself, while the real entry point sat just past it. Skip the fill; the entry
     *  is what follows it. */
    private long skipFill(long lo, long hi) throws Exception {
        long va = lo;
        while (va < hi) {
            int n = fillWidth(va, hi);
            if (n == 0) return va;
            va += n;
        }
        return -1;
    }

    /** Width of the alignment fill at va, or 0 if the bytes there are not fill. */
    private int fillWidth(long va, long hi) throws Exception {
        int b0 = u8(va);
        if (b0 == 0xCC || b0 == 0x90 || b0 == 0x00) return 1;
        if (va + 1 < hi && b0 == 0x8B && u8(va + 1) == 0xFF) return 2;          // mov edi,edi
        if (va + 1 < hi && b0 == 0x66 && u8(va + 1) == 0x90) return 2;          // xchg ax,ax
        if (va + 2 < hi && b0 == 0x8D && u8(va + 1) == 0x49 && u8(va + 2) == 0x00)
            return 3;                                                          // lea ecx,[ecx+0]
        if (va + 6 < hi && b0 == 0x8D && u8(va + 1) == 0xA4 && u8(va + 2) == 0x24
                && u8(va + 3) == 0 && u8(va + 4) == 0 && u8(va + 5) == 0 && u8(va + 6) == 0)
            return 7;                                                          // lea esp,[esp+0]
        return 0;
    }

    private int u8(long va) throws Exception {
        return currentProgram.getMemory().getByte(toAddr(va)) & 0xFF;
    }

    /** True when the bytes read as a table of code pointers rather than as code.
     *
     *  MSVC emits switch jump tables INTO .text, and an entry in one carries a DATA
     *  reference -- so "something stores this address" is satisfied by a jump-table slot,
     *  and the sweep's first pass duly turned 0x004668B0 (`ae 68 46 00 | 46 68 46 00` --
     *  three consecutive .text addresses) into a 1-byte "function". A run whose first two
     *  dwords are both .text addresses is a table, not an entry point. */
    private boolean looksLikePointerTable(long va) throws Exception {
        long lo = imageMin(), hi = imageMax();
        for (int i = 0; i < 2; i++) {
            long d = dword(va + 4L * i);
            if (d < lo || d >= hi) return false;
        }
        return true;
    }

    private long dword(long va) throws Exception {
        return ((long) u8(va)) | ((long) u8(va + 1) << 8)
                | ((long) u8(va + 2) << 16) | ((long) u8(va + 3) << 24);
    }

    private long imageMin() {
        return currentProgram.getImageBase().getOffset();
    }

    private long imageMax() {
        long hi = imageMin();
        for (MemoryBlock b : currentProgram.getMemory().getBlocks())
            if (b.isExecute() && b.isInitialized())
                hi = Math.max(hi, b.getEnd().getOffset() + 1);
        return hi;
    }

    /** True when the created body actually terminates -- a function returns or tail-jumps.
     *
     *  A 15-byte "function" of decoded garbage that simply runs on into the next function's
     *  entry (0x00465441, which ends exactly at _CTEval_maxspeeddiff) never terminates. Real
     *  code does. This is the last gate before a fragment becomes a fact. */
    private boolean returns(Function f) {
        if (f.getBody().getNumAddresses() < 8) return false;
        for (Instruction in : currentProgram.getListing().getInstructions(f.getBody(), true)) {
            FlowType t = in.getFlowType();
            if (t.isTerminal() || (t.isJump() && t.isUnConditional())) return true;
        }
        return false;
    }

    /** "REF" / "PROLOGUE", or null when nothing says this is a function ENTRY.
     *
     *  A CALL proves an entry. So does a DATA reference: something stores the address, which
     *  is how a proc-table entry or callback is reached in the first place.
     *
     *  A JUMP proves nothing. Its target is just as likely a switch case or a loop head
     *  INSIDE an existing function -- and promoting one to a function entry manufactures a
     *  fragment: the first pass did exactly that to 0x00465441 and 0x004668B0, mid-function
     *  jump targets in the CT interpreter whose "bodies" decompiled into the verbatim tails
     *  of _CTEval_maxspeeddiff and CTRestoreState. Fallthrough is rejected for the same
     *  reason -- code that runs on into the next function is that function's tail, not a new
     *  one. A wrong function is worse than an undefined gap: it inflates the denominator with
     *  fiction and then reports coverage against it. */
    private String evidence(long va) throws Exception {
        Address a = toAddr(va);

        for (Reference ref : currentProgram.getReferenceManager().getReferencesTo(a)) {
            RefType t = ref.getReferenceType();
            if (t.isCall() || t.isData()) return "REF";
        }

        byte[] b = new byte[4];
        currentProgram.getMemory().getBytes(a, b);
        int b0 = b[0] & 0xFF, b1 = b[1] & 0xFF, b2 = b[2] & 0xFF;
        // The prologues this MSVC build actually emits. NOT 8B FF (`mov edi,edi`) -- in this
        // toolchain that is inter-function alignment fill, not a hot-patch prologue; see
        // skipFill().
        if (b0 == 0x55 && b1 == 0x8B && b2 == 0xEC) return "PROLOGUE";   // push ebp; mov ebp,esp
        if (b0 == 0x83 && b1 == 0xEC) return "PROLOGUE";                 // sub esp, imm8
        if (b0 == 0x81 && b1 == 0xEC) return "PROLOGUE";                 // sub esp, imm32
        if (b0 == 0x64 && b1 == 0xA1) return "PROLOGUE";                 // mov eax, fs:[0]  (SEH)
        return null;
    }
}
