// Export the Ghidra-project ground truth for the reconstruction program (#209/#247).
// Scoped to the current binary (this program's name, e.g. FA.EXE / WAIL32.DLL);
// image bounds come from the program, not a hardcoded window. Writes three
// byte-stable CSVs into <repo>/db/inventory/<binary>/:
//   functions.csv  va,name,size                 -- every function in the image
//   globals.csv    va,name,xref_count,subsystems,widths,indexed -- data syms with >=1 code xref
//   ranges.csv     slug,range,bytes,bytes_in_functions,functions -- per manifest range
//   callsites.csv  callee_va,site_va,cleanup_bytes,pushes_before -- caller-side evidence (#453)
//   frames.csv     va,ebp_frame,ret_imm,max_stack_arg,max_stack_ref,reads_ecx,reads_edx (#453)
//
// Subsystem tags come from <repo>/db/subsystems.csv (ranges) and
// <repo>/db/symbols/*.csv (explicit claims; a claim overrides range membership).
//
// Invoke: scripts/ghidra/export_inventory.sh  (passes the repo root as scriptArg)
// The output is LOCAL-ONLY and never committed (#342); see db/README.md for the
// canonical-project rule.
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
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.pcode.PcodeOp;
import ghidra.program.model.pcode.Varnode;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.ReferenceManager;
import ghidra.program.model.symbol.StackReference;
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
            w.write("va,name,xref_count,subsystems,widths,indexed\n");
            for (Map.Entry<Long, String> e : dataNames.entrySet()) {
                long va = e.getKey();
                int xrefs = 0;
                TreeSet<String> tags = new TreeSet<>();
                // Evidence for the global typing pass (#455), straight off the instructions:
                //   widths  -- the operand size of every access. `MOV AL,[x]` PROVES a byte is
                //              read there; `MOV EAX,[x]` proves a dword. Disagreement is not
                //              noise -- it means the address is not a plain scalar.
                //   indexed -- whether any access reaches the address through a register, i.e.
                //              `[base + reg]`. That makes it an ARRAY base, and typing an array
                //              as a scalar would hand the port the wrong size.
                TreeSet<Integer> widths = new TreeSet<>();
                boolean indexed = false;
                for (Reference ref : rm.getReferencesTo(toAddr(va))) {
                    Function from = fm.getFunctionContaining(ref.getFromAddress());
                    if (from == null) continue; // only code xrefs count
                    xrefs++;
                    String tag = membership(from.getEntryPoint().getOffset(),
                                            subsystems, claims);
                    if (tag != null) tags.add(tag);

                    Instruction in = getInstructionAt(ref.getFromAddress());
                    if (in == null) continue;
                    int wsz = accessWidth(in, va);
                    if (wsz > 0) widths.add(wsz);
                    if (isIndexed(in, ref)) indexed = true;
                }
                if (xrefs == 0) continue;
                StringBuilder ws = new StringBuilder();
                for (int wv : widths) {
                    if (ws.length() > 0) ws.append("|");
                    ws.append(wv);
                }
                w.write(String.format("0x%08X,%s,%d,%s,%s,%b\n",
                        va, csv(e.getValue()), xrefs, String.join(";", tags),
                        ws.toString(), indexed));
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

        // --- callsites.csv --------------------------------------------------
        // Evidence for the cdecl signature recovery in tools/recover_signatures.py (#453).
        //
        // A cdecl callee cleans nothing (`ret 0`), so its own code proves no argument count.
        // The proof is at the call site: a cdecl caller pops its own arguments right after
        // the CALL, with `ADD ESP, N` -- or, for a single dword, MSVC's `POP ECX` idiom.
        // Because cdecl passes NO arguments in registers, that byte count is the FULL arity,
        // with nothing hidden. (Every register-based scheme we measured -- caller-side and
        // callee-side alike -- misjudged fastcall arguments 8-16% of the time, which is why
        // only this one is exported as evidence. See db/types/README.md.)
        //
        // The raw observation is exported and no conclusion drawn: the consensus rule lives
        // in tools/, because "every call site agrees" is the entire basis for trusting it.
        int siteCount = 0;
        try (BufferedWriter w = open(new File(outDir, "callsites.csv"))) {
            w.write("callee_va,site_va,cleanup_bytes,pushes_before\n");
            for (Map.Entry<Long, Function> e : functions.entrySet()) {
                Address entry = toAddr(e.getKey());
                ReferenceIterator ri =
                        currentProgram.getReferenceManager().getReferencesTo(entry);
                while (ri.hasNext()) {
                    Reference ref = ri.next();
                    if (!ref.getReferenceType().isCall()) continue;
                    Instruction call = getInstructionAt(ref.getFromAddress());
                    if (call == null) continue;
                    w.write(String.format("0x%08X,0x%08X,%d,%d\n",
                            e.getKey(), ref.getFromAddress().getOffset(),
                            cleanupAfter(call), pushesBefore(call)));
                    siteCount++;
                }
            }
        }
        println("callsites.csv: " + siteCount + " call sites");

        // --- frames.csv -------------------------------------------------------
        // Callee-side evidence for the #453 TAIL: the functions whose call sites show no
        // stack cleanup at all (MSVC merged or deferred it), so the caller-side rule that
        // recovered the cdecl arities has nothing to read.
        //
        // Three OBSERVATIONS per function, no conclusions -- the rule, and the burden of
        // proving it, live in tools/recover_frames.py:
        //
        //   ret_imm         the RET's actual operand. NOT Ghidra's getStackPurgeSize(),
        //                   which is an INFERENCE (it disagreed with the real operand 179
        //                   times, and answers 2147483647 for "unknown"). `ret 0` means the
        //                   callee cleans nothing; `ret N` means it cleans N.
        //   max_stack_arg   the highest [EBP + N] byte offset the function READS, for N >= 8
        //                   -- i.e. above the saved EBP and return address, which is where
        //                   incoming stack arguments live. Only meaningful with an EBP frame,
        //                   so ebp_frame is reported alongside and an FPO function yields -1.
        //   reads_ecx/edx   whether the function reads ECX/EDX as an INPUT before writing it.
        //
        // The last one exists to DISQUALIFY, never to count. Every attempt to infer arity
        // FROM register use failed (7.6-16% wrong, see db/types/README.md), because you
        // cannot tell an incoming register argument from a scratch use. But the converse is
        // sound and is all that is needed here: a function that touches ECX/EDX as an input
        // MIGHT be taking arguments in them, so its stack reads cannot be the whole story --
        // refuse it. Refusing on a maybe costs recall; counting on a maybe costs correctness.
        int frameCount = 0;
        try (BufferedWriter w = open(new File(outDir, "frames.csv"))) {
            w.write("va,ebp_frame,ret_imm,max_stack_arg,max_stack_ref,reads_ecx,reads_edx\n");
            for (Map.Entry<Long, Function> e : functions.entrySet()) {
                Frame f = frameOf(e.getValue());
                w.write(String.format("0x%08X,%b,%d,%d,%d,%b,%b\n",
                        e.getKey(), f.ebpFrame, f.retImm, f.maxStackArg, f.maxStackRef,
                        f.readsEcx, f.readsEdx));
                frameCount++;
            }
        }
        println("frames.csv: " + frameCount + " functions");
    }

    /** Callee-side frame observations. See the frames.csv note above. */
    private static class Frame {
        boolean ebpFrame = false;
        int retImm = -1;        // -1: no RET seen, or RETs disagree
        int maxStackArg = -1;   // -1: no EBP frame, so not measurable
        int maxStackRef = -1;   // highest positive normalized stack offset referenced
        boolean readsEcx = false;
        boolean readsEdx = false;
    }

    private Frame frameOf(Function fn) {
        Frame f = new Frame();
        List<Instruction> body = new ArrayList<>();
        for (Instruction in : currentProgram.getListing().getInstructions(fn.getBody(), true)) {
            body.add(in);
        }
        if (body.isEmpty()) return f;

        // A standard MSVC prologue: PUSH EBP ; MOV EBP,ESP. Without it the function
        // addresses its arguments off ESP, which shifts as the frame moves -- not
        // measurable from a static scan, so we report nothing rather than a wrong number.
        if (body.size() >= 2
                && body.get(0).getMnemonicString().equalsIgnoreCase("PUSH")
                && "EBP".equalsIgnoreCase(body.get(0).getDefaultOperandRepresentation(0))
                && body.get(1).getMnemonicString().equalsIgnoreCase("MOV")
                && "EBP".equalsIgnoreCase(body.get(1).getDefaultOperandRepresentation(0))
                && "ESP".equalsIgnoreCase(body.get(1).getDefaultOperandRepresentation(1))) {
            f.ebpFrame = true;
            f.maxStackArg = 0;
        }

        boolean ecxWritten = false, edxWritten = false;
        boolean retSeen = false;
        for (Instruction in : body) {
            String mn = in.getMnemonicString().toUpperCase();

            if (mn.startsWith("RET")) {
                int imm = 0;
                Object[] ops = in.getOpObjects(0);
                if (ops.length > 0 && ops[0] instanceof Scalar) {
                    imm = (int) ((Scalar) ops[0]).getUnsignedValue();
                }
                if (!retSeen) {
                    f.retImm = imm;
                    retSeen = true;
                } else if (f.retImm != imm) {
                    f.retImm = -1;  // inconsistent epilogues: no answer
                }
            }

            // Incoming stack arguments live at [EBP + 8] and up (past saved EBP + return
            // address). Only READS count: a write to [EBP+8] is the callee overwriting its
            // own argument slot, which still proves the slot exists -- but a read is the
            // unambiguous "this argument is consumed" signal, and staying strict here keeps
            // the floor honest.
            if (f.ebpFrame) {
                int off = ebpArgOffset(in);
                if (off >= 8 && off > f.maxStackArg) f.maxStackArg = off;
            }

            // The same question, asked of Ghidra's stack model rather than of EBP. FA.EXE is
            // built with frame-pointer omission -- only 26 of the 733 unsigned functions have
            // an EBP frame at all -- so arguments are addressed off a MOVING ESP, and no
            // static scan of the operand can name the slot. Ghidra tracks the stack depth
            // through the function and normalizes such an access into a stack offset where
            // positive offsets are the CALLER's frame, i.e. the incoming arguments.
            //
            // This is an INFERENCE, not an encoded fact, and this database has been burned by
            // Ghidra inferences before (getStackPurgeSize disagreed with the real RET operand
            // 179 times). So it is exported as an observation and MEASURED in tools/ against
            // the signatures the FA.SMS decorations prove independently. It earns its place or
            // it does not.
            for (Reference r : in.getReferencesFrom()) {
                if (!r.isStackReference()) continue;
                int off = ((StackReference) r).getStackOffset();
                if (off > 0 && off > f.maxStackRef) f.maxStackRef = off;
            }

            // Register inputs, tracked only until the register is first written.
            if (!ecxWritten && readsReg(in, "ECX") && !selfClear(in, "ECX")) f.readsEcx = true;
            if (!edxWritten && readsReg(in, "EDX") && !selfClear(in, "EDX")) f.readsEdx = true;
            if (writesReg(in, "ECX")) ecxWritten = true;
            if (writesReg(in, "EDX")) edxWritten = true;
        }
        return f;
    }

    /** The `N` of an `[EBP + N]` memory operand this instruction READS, or -1. */
    private int ebpArgOffset(Instruction in) {
        int best = -1;
        for (int i = 0; i < in.getNumOperands(); i++) {
            Object[] objs = in.getOpObjects(i);
            boolean hasEbp = false;
            long disp = 0;
            boolean hasDisp = false;
            for (Object o : objs) {
                if (o instanceof Register && "EBP".equalsIgnoreCase(((Register) o).getName())) {
                    hasEbp = true;
                } else if (o instanceof Scalar) {
                    disp = ((Scalar) o).getSignedValue();
                    hasDisp = true;
                } else if (o instanceof Register) {
                    // [EBP + reg*n]: an indexed access into an array argument. The base
                    // offset still names the argument slot, so it is not disqualifying,
                    // but a second register means we are not reading a plain slot -- skip.
                    return -1;
                }
            }
            if (hasEbp && hasDisp && disp > 0 && disp < 0x1000 && disp > best) {
                best = (int) disp;
            }
        }
        return best;
    }

    /** Register match by BASE register, so CX / CL count as touching ECX.
     *
     *  This is not a detail: a __fastcall callee taking a `ushort` receives it in CX, and an
     *  exact-name comparison sees no ECX anywhere in the function. Measured against the known
     *  signatures, that single leak was the ONLY false positive the callee-cleans rule
     *  produced -- it typed @Reaction@12 (fastcall, 3 args) as stdcall/1. Comparing base
     *  registers closes it.
     */
    private boolean touchesReg(Object o, String reg) {
        if (!(o instanceof Register)) return false;
        Register r = (Register) o;
        Register base = r.getBaseRegister();
        return reg.equalsIgnoreCase(r.getName())
                || (base != null && reg.equalsIgnoreCase(base.getName()));
    }

    private boolean readsReg(Instruction in, String reg) {
        for (Object o : in.getInputObjects()) {
            if (touchesReg(o, reg)) return true;
        }
        return false;
    }

    private boolean writesReg(Instruction in, String reg) {
        for (Object o : in.getResultObjects()) {
            if (touchesReg(o, reg)) return true;
        }
        return false;
    }

    /** `XOR ECX,ECX` / `SUB ECX,ECX` -- the register appears as an input, but this is a
     *  write idiom, not a read of an incoming value. */
    private boolean selfClear(Instruction in, String reg) {
        String mn = in.getMnemonicString().toUpperCase();
        if (!mn.equals("XOR") && !mn.equals("SUB")) return false;
        return in.getNumOperands() == 2
                && reg.equalsIgnoreCase(in.getDefaultOperandRepresentation(0))
                && reg.equalsIgnoreCase(in.getDefaultOperandRepresentation(1));
    }

    /** Consecutive PUSHes immediately preceding this CALL -- the caller's own view of how
     *  many stack arguments it is passing, independent of anything the callee does.
     *
     *  MSVC also stages arguments with `SUB ESP,N` + `MOV [ESP+k]`, which pushes nothing.
     *  Such a site yields 0 and is treated by tools/ as NO EVIDENCE, not as "zero arguments"
     *  -- the same silence-over-guess rule as cleanupAfter().
     */
    private int pushesBefore(Instruction call) {
        int n = 0;
        Instruction in = call.getPrevious();
        while (in != null && in.getMnemonicString().equalsIgnoreCase("PUSH") && n < 32) {
            n++;
            in = in.getPrevious();
        }
        return n;
    }

    /** The operand size of this instruction's direct access to `va`, or 0 if unknown (#455).
     *
     *  A direct absolute access (`MOV AL,[0x50D08C]`) lowers to a p-code varnode living in the
     *  RAM space at that address, whose SIZE is exactly the width the instruction touches --
     *  1 for AL, 2 for AX, 4 for EAX. That is a fact carried by the encoded instruction, not
     *  an inference about it.
     *
     *  An INDEXED access (`MOV EAX,[0x50D08C + ECX*4]`) is deliberately NOT measured here: the
     *  base appears only as a CONSTANT feeding an address computation, so its varnode size is
     *  the pointer width (4) and has nothing to do with the element width. Reporting that as
     *  the access width would type every array as a dword. Unknown is the honest answer;
     *  isIndexed() flags the address separately.
     */
    private int accessWidth(Instruction in, long va) {
        for (PcodeOp op : in.getPcode()) {
            for (Varnode v : op.getInputs()) {
                if (isRamAt(v, va)) return v.getSize();
            }
            Varnode out = op.getOutput();
            if (isRamAt(out, va)) return out.getSize();
        }
        return 0;
    }

    private boolean isRamAt(Varnode v, long va) {
        return v != null && v.isAddress() && v.getAddress().getOffset() == va;
    }

    /** Does this instruction reach the address through a register -- i.e. `[base + reg]`?
     *
     *  A register in the memory operand means the address is being INDEXED, which makes it an
     *  array base rather than a scalar. Typing an array as a scalar would give the port an
     *  object of the wrong size, so #455 refuses these outright.
     */
    private boolean isIndexed(Instruction in, Reference ref) {
        int opIdx = ref.getOperandIndex();
        if (opIdx < 0 || opIdx >= in.getNumOperands()) return false;
        for (Object o : in.getOpObjects(opIdx)) {
            if (o instanceof Register) return true;
        }
        return false;
    }

    /** Bytes the caller pops immediately after this CALL, or -1 if it cannot be read off.
     *
     *  MSVC often merges the cleanup of several consecutive calls into one ADD ESP, or defers
     *  it entirely. Such a site yields NO evidence (-1) rather than a wrong small number --
     *  silence, not a guess. tools/ then refuses any callee whose sites do not all agree.
     */
    private int cleanupAfter(Instruction call) {
        Instruction in = call.getNext();
        if (in == null) return -1;
        String mn = in.getMnemonicString().toUpperCase();

        if (mn.equals("ADD") && "ESP".equalsIgnoreCase(in.getDefaultOperandRepresentation(0))) {
            Object[] ops = in.getOpObjects(1);
            if (ops.length > 0 && ops[0] instanceof Scalar) {
                return (int) ((Scalar) ops[0]).getUnsignedValue();
            }
            return -1;
        }
        // `POP ECX` / `POP EDX` -- MSVC's idiom for discarding a single dword argument.
        if (mn.equals("POP")) {
            String r = in.getDefaultOperandRepresentation(0);
            if ("ECX".equalsIgnoreCase(r) || "EDX".equalsIgnoreCase(r)) return 4;
        }
        return -1;
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
