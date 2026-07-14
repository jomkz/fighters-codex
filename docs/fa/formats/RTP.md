---
format: RTP
name: RTPatch Binary Patch
extensions: [".RTP"]
category: installer
endianness: little
spec:
  status: partial
  gaps:
    - kind: re-static
      issue: 54
      note: "several reserved header and per-record metadata words, and the combine identifier, are consumed structurally but not interpreted"
codec:
  direction: read
  rationale: "decode + apply only — reconstructs the 1.02F target from the 1.00F source plus the patch. fx_lib has no RTPatch encoder (authoring a patch needs Pocket Soft's proprietary differ), so a writer would prove nothing; the byte-exact proof is that the reconstructed output matches the 1.02F build."
  lib: [lib/src/rtpatch.cpp]
  commands: [patch]
  tests: [tests/test_rtpatch.cpp]
  fuzz: [fuzz/fuzz_rtpatch.cpp]
  fixtures:
    synthetic: true
    real_manifest: false
    real_install: false
related: [ESA, SSF]
credits:
  - "0xB59C codec, §9 opcode grammar, and §10 rolling checksum reverse-engineered by the MIT-licensed rtptool project (github.com/bwrsandman/rtptool, © Sandy Carter); this codec is a clean-room port of those facts"
---

# RTP — RTPatch Binary Patch (.RTP)

The retail discs ship Fighters Anthology build **1.00F**, while every recovered
symbol and format in this project describes the patched **1.02F** build. The gap
is closed by one downloadable updater, `fae102.exe`, which carries a **Pocket
Soft .RTPatch Professional 4.11** payload as an overlay. Reversing that payload —
not the third-party patcher binary — lets `fx install` produce a 1.02F tree from
the user's own discs.

The patch carries eight file records. Six are **MODIFY** — a binary diff against
the 1.00F original: `FA.EXE`, `FA.SMS`, `FA_1.LIB`, `FA_2.LIB`, `README.TXT`, and
the installer's own `EAEXEC.EXE`. Two are **NEW** — a full file delivered whole:
`msapi.dll` (the online-matchmaking API) and a small `ealtest.exe`. Each file is
carried by the same custom codec — an order-0 adaptive Huffman over an LZSS token
stream, tagged `0xB59C`, MSB-first. A MODIFY record decompresses to a program of
copy / poke / fill opcodes (§ Engine Notes) applied against the 1.00F original; a
NEW record decompresses straight to the file. The reconstruction is byte-identical
to the shipped 1.02F build.

`EAEXEC.EXE` and `ealtest.exe` are flagged for a prompted **system** directory
(`\WINDOWS\SYSTEM`), not the game directory; `fx install --patch` applies only the
app-directory records.

> **No `FX_FA_ROOT` census, by nature.** The `.RTP` patch container ships on the **retail disc**, not in an installed game. It is exercised against real media by the `fa_disc_install` CTest (the 1.00F → 1.02F patch chain). See [#491](https://github.com/jomkz/fighters-codex/issues/491).

## Tools

### fx

```
fx patch inspect <patch.exe>
fx patch apply   <patch.exe> --source <dir> --out <dir> [--file NAME] [--no-checksum]
```

`inspect` locates the container overlay and lists every record: name, mode,
source and target sizes, and the source checksum. `apply` reconstructs each file
from the matching original in `--source` and writes it to `--out`; the source is
verified against the record's §10 checksum first, and a mismatch skips the file
(the wrong version would otherwise yield a correct-sized but corrupt result).
`--no-checksum` forces the apply; `--file` limits it to one target.

```
$ fx patch inspect fae102.exe
container:  RTPatch v0x019a (extra mode) @ offset 0x2b600
system files (installer-prompted):
  EAEXEC.EXE      Location of your \WINDOWS\SYSTEM directory

File            Mode          Source        Target  Source checksum
FA.EXE          modify       1299968       1319424  w1=5b01cc0f w2=3f326589
...
```

## File Layout

The payload is a self-contained `.RTPatch` container (FA carries it as an overlay
inside the updater `.EXE` at file offset `0x2b600`; `fx patch` scans for the
`K*` magic). All fixed integers are little-endian.

```
+-------------------+
| header            |  "K*", version, flags, sizes (see below)
+-------------------+
| special-files tbl |  count + (name, prompt) lp_strings — files the installer
|                   |  relocates to a system dir (EAEXEC.EXE → \WINDOWS\SYSTEM)
+-------------------+
| directory table   |  count + lp_strings (empty in FA)
+-------------------+
| file record 0     |
| file record 1     |
| ...               |
| EOF record        |  rec_hdr type nibble == 1
+-------------------+
```

Three encodings coexist: fixed-width little-endian integers (header, entry
descriptors); a byte-oriented **VLI** for opcode operands and counts; and an
MSB-first **bit stream** for the compressed diff (§ File Inventory).

**Header** — `magic "K*"`, `version u16`, `flags u16`; if `flags` bit 15 is set an
`ext_type_flags u32` follows (bit 16 = *extra mode*, which adds timestamp and
alt-path fields to every entry); then `option_flags u16`, `patch_total_size u32`,
reserved words, `cmd_flags u16` (bit 2 gates a `combine_id u32`), and a final
reserved word. FA's header is `version 0x019a`, extra mode on.

**lp_string** — `u8 length` (including the NUL; `0xFF` escapes to a following
`u16`), then that many bytes ending in a NUL.

**VLI** — one lead byte: bit 7 is the sign; the count of 1-bits from bit 6
downward gives the number of little-endian continuation bytes. `count == 0` holds
a value `0..63` in the low 6 bits; otherwise the lead byte's remaining low bits
are the most-significant part.

**File record** — a `rec_hdr u16` whose top nibble is the record type and whose
low bits gate optional fields (option override, inline name, disk-set, attributes,
explicit paths), then a 10-byte metadata block, then the type's data block. FA
uses two types: **type 4 (MODIFY)**, a binary diff, and **type 2 (NEW)**, a full
compressed file. (Bit 7, the disk-set flag, marks a system-directory file —
`EAEXEC.EXE`, `ealtest.exe` — versus an app-directory one.)

- A **MODIFY** block is `file_mod_flags u16`, `src_count` and `dst_count` VLIs, a
  reserved `u32`, a `payload_len u32`, then the source and destination **entry
  descriptors**, then the compressed diff (`payload_len` bytes).
- A **NEW** block is `src_count` VLI, `usize u32` (decompressed size), `csize u32`
  (the compressed length), then the entry descriptor(s), then the compressed file
  (`csize` bytes). Records are contiguous, so `block_off + csize` is the next
  record — the walk covers all eight without decoding anything.

**Entry descriptor** — a 24-byte descriptor (8.3 name; the file size is a `u32` at
offset 16), a 10-byte checksum block (§10, below), and in extra mode 8 timestamp
bytes plus an alt-path lp_string carrying the full filename. The reconstructed
size is the first destination entry's file size.

## File Inventory

The compressed diff is a single MSB-first bit stream:

| Bits | Field        | Meaning |
|------|--------------|---------|
| 16   | magic        | `0xB59C` |
| 8    | literal_mode | 0 → literals use adaptive Huffman; nonzero → raw 8-bit literals |
| 8    | reserved     | consumed, discarded |
| 12   | init_period  | Huffman frequency-reset period |
| 12   | upd_period   | Huffman update period |
| 4    | window_flag  | `8` → 8 KB window / 7 distance low-bits; else 4 KB / 6 |

Three adaptive-Huffman alphabets follow — literal (256 symbols), length (64),
distance (64) — all sharing the one bit cursor. Each is an order-0 adaptive model
(a level / group / slot canonical structure with periodic weight-halving rebuild);
an **escape** symbol introduces an unseen literal by reading a fixed number of raw
bits. FA's msapi header decodes to `literal_mode=0`, `init_period=64`,
`upd_period=16`, `window_flag=8`.

**Token loop.** Read one flag bit. `0` → a literal (adaptive-Huffman symbol, or 8
raw bits when `literal_mode != 0`); emit the byte. `1` → a back-reference:
`dist_lo` = *window* raw bits, `dist_hi` = a distance-alphabet symbol,
`dist = (dist_hi << window) | dist_lo`; **`dist == 0` ends the stream**; otherwise
`length` = a length-alphabet symbol masked to 7 bits, and `length` bytes are
copied from `output[pos − (dist + 1)]`, reading zero when `pos < dist + 1` (a
zero-initialised window, needed for the long zero runs in a PE header). For a NEW
record these bytes are the file; for a MODIFY record they are the opcode stream.

## Engine Notes

`fx patch` (`lib/src/rtpatch.cpp`) executes the reconstruction: it is what proves
the format understood — the byte-identical output is the proof. A MODIFY record's
decompressed bytes are a program (§9) that rebuilds the destination against the
source. The interpreter keeps a **write cursor**, a separate **poke cursor**, a
**gap list** of literal holes, a **template** dictionary, and a zero-initialised
output buffer of the destination size.

| Op   | Name       | Effect |
|------|------------|--------|
| 0x01 | END        | terminate |
| 0x02 | SET_SOURCE | select source; reset write and poke cursors |
| 0x03/0x04 | COPY (+gap) | copy `cnt` source bytes at an absolute offset; a leading advance records a gap first |
| 0x05 | FLUSH      | record a final gap to end-of-file, then fill **every** gap in order with literal bytes from the stream |
| 0x06 | POKE1      | poke cursor += seek; add a signed delta to the byte there |
| 0x07/0x0D/0x0E | POKE1×N | N running 1-byte delta-adds (constant or per-entry delta) |
| 0x08 | STORE      | append a `{source, offset, count}` copy template (emits nothing) |
| 0x09/0x0A | TCOPY (+gap) | COPY through a stored template |
| 0x0B/0x0C | ZFILL (+gap) | write a run of zero bytes |
| 0x0F/0x10 | POKE16/32×N | N running 2- or 4-byte little-endian delta-adds |
| 0x11–0x16 | FILL1/2/4 (+gap) | repeat a 1/2/4-byte pattern for a byte count |

Copies address the source at an **absolute** offset. Pokes are **delta-adds, not
overwrites** — the little-endian value at the poke cursor is incremented; the
cursor accumulates within an op and resets at SET_SOURCE and the multi-poke ops
(but not 0x06). Unchanged regions are emitted as copies, leaving holes that the
single late FLUSH backfills with the stream's remaining literal bytes.

**Source validation (§10).** Each source entry carries a dual rolling checksum.
Both start at zero and fold each byte as `w = rotl8(w ^ c)` within N bits — `w1`
at 31 bits (`0x7FFFFFFF`), `w2` at 30 bits (`0x3FFFFFFF`). `fx patch apply`
verifies the on-disk source's `w1`/`w2` before applying, so a wrong 1.00F version
is refused rather than silently corrupted.

**Validated.** The `fa_patch_apply` integration test (behind `FX_FA_PATCH`) applies
`fae102.exe` to the ESA-sourced 1.00F originals and checks each output's SHA-256
against a committed manifest: `FA.SMS`, `FA_1.LIB`, `FA_2.LIB`, the added
`msapi.dll`, and the **official** `FA.EXE` all reconstruct byte-for-byte (the first
four also match a licensed install). A licensed install's `FA.EXE` may differ by a
byte if it carries a no-CD crack — a `JNZ`→`JZ` flip in the CD check — which is a
property of that install, not of the patch.

## Open Questions

- Several reserved header words, the per-record 10-byte metadata block, and the
  `combine_id` are consumed structurally but not interpreted. [#54]
- fx_lib decodes and applies patches; it has no encoder, so the format is
  documented `read`-only (authoring needs Pocket Soft's differ).

## Related

- [ESA](ESA.md) — the Disc 1 installer archive that supplies the 1.00F originals
  the patch is applied against.
- [SSF](SSF.md) — the installer scripts `fx install` runs; `fx install --patch`
  chains this codec after a 1.00F install to reach 1.02F.
