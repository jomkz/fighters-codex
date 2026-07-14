---
format: ESA
name: EA Installer Archive
extensions: [".ESA"]
category: installer
endianness: little
spec:
  status: partial
  gaps:
    - kind: re-static
      issue: 54
      note: "entry flags bitfield (0x211 vs 0x221) semantics inferred from the SSF INSTALL_SYSFILES correlation, not yet confirmed in SETUP.EXE"
codec:
  direction: round-trip
  byte_identical: true
  lib: [lib/src/esa.cpp, lib/src/blast.cpp]
  commands: [esa]
  tests: [tests/test_esa.cpp]
  fuzz: [fuzz/fuzz_esa.cpp]
  fixtures:
    synthetic: true
    real_manifest: false
    real_install: false
related: [LIB, SSF, RGN]
credits:
  - "PKWare DCL decode based on blast.c by Mark Adler (zlib project)"
---

# ESA — EA Installer Archive (.ESA)

`SETUP.ESA` is the single archive on the FA Disc 1 root that carries every file
the EA installer copies to disk: the game executable and symbols (`FA.EXE`,
`FA.SMS`), the installed LIB archives (`FA_1/2/4B/4D.LIB`), the sound and comms
drivers, the tech-support tool, and the loose install text. The installer
(`SETUP.EXE`) reads it under the direction of the `.SSF` scripts
([SSF.md](SSF.md)), whose `INSTALL_FILES` directives select entries by the
`label` each carries. There is one `.ESA` in FA; the CD-resident `FA_4C.LIB` and
`FA_7.LIB` sit loose beside it and are not packed in.

The container is a flat directory of variable-length records followed by the
payloads back to back — no index, no padding, no per-payload header. `PKWA`
entries are raw PKWare DCL streams and `NULL` entries are stored; the four LIB
archives are stored, so extracting them yields byte-identical LIBs that parse
straight through [the LIB codec](LIB.md).

> **No `FX_FA_ROOT` census, by nature.** `.ESA` is the installer's own archive: it exists on the **retail disc**, not in an installed game. It *is* exercised against real media — the `fa_disc_install` CTest (`-DFX_FA_DISC1=`/`-DFX_FA_DISC2=`) checks its manifest and a byte-identical repack. See [development.md](https://github.com/jomkz/fighters-codex/blob/main/docs/development.md) § Real-media install mode, and [#491](https://github.com/jomkz/fighters-codex/issues/491).

## Tools

### fx

```
fx esa ls      <SETUP.ESA>              # directory: name, label, flags, method, sizes
fx esa info    <SETUP.ESA>              # entry/method counts, directory size, repack check
fx esa extract <SETUP.ESA> <NAME> [-o]  # one entry (PKWA decoded, NULL copied)
fx esa unpack  <SETUP.ESA> [-o dir]     # every entry
fx esa repack  <SETUP.ESA> <out.ESA>    # container round-trip (byte-identical)
```

`fx esa` is a thin front-end over `fx::esa_read_dir` / `esa_extract` / `esa_repack`
([api.md](../../api.md#esah--ea-installer-archive)). The PKWare DCL decode reuses
`fx::blast_decompress` — the same decoder the LIB codec uses — because a `.ESA`
`PKWA` stream is *raw* DCL.

## File Layout

```
magic  char[29]   "ELECTRONIC_ARTS_ARCHIVE_FILE\0"
dir    record[]    variable-length records, back to back
term   u8          0x00 — an empty (zero-length) name ends the directory
data   blob[]      payloads, contiguous, in directory order; the first begins
                   at the byte after the terminator, the last ends at EOF
```

### Directory record — confirmed

All integers are little-endian; strings are NUL-terminated and are **not** 8.3 —
names may hold spaces and apostrophes (`JANE'S HOME PAGE.URL`).

| Field | Type | Description |
|-------|------|-------------|
| `name` | `char[]` | file name, NUL-terminated |
| `label` | `char[]` | archive label — the token `.SSF` `INSTALL_FILES` selects on |
| `flags` | `u32` | see below |
| `usize` | `u32` | uncompressed size |
| `mtime` | `u32` | modification time (Unix epoch) |
| `method` | `char[]` | `"PKWA"` (PKWare DCL) or `"NULL"` (stored), NUL-terminated |
| `csize` | `u32` | stored size; for `NULL`, `csize == usize` |
| `offset` | `u32` | absolute payload offset within the archive |

The directory self-terminates on an empty name, so there is no count field: the
byte after the terminator is the first payload offset. Two invariants hold across
a well-formed archive and are enforced on read — the directory does not overlap
any payload (`offset >= directory_size`), and the payloads exactly fill the file
(`max(offset + csize) == file size`). On the retail `SETUP.ESA` the last entry
ends at byte 109,979,167, the archive's exact length.

### Entry flags — inferred

Every retail entry is `0x0211`, except `EAREMOVE.EXE` and `EAEXEC.EXE`, which are
`0x0221` — and those are exactly the two the `.SSF` scripts install with
`INSTALL_SYSFILES` (to the Windows system directory) rather than `INSTALL_FILES`
(to the app directory). The differing bit (`0x10` vs `0x20`) therefore reads as a
destination-class selector. The remaining low bits (`0x001`, `0x200`) are
unexplained. Confirming the bitfield is a target of the SETUP.EXE reconstruction
(#54); the codec preserves `flags` verbatim regardless.

### Compression — confirmed

`PKWA` payloads are **raw** PKWare DCL streams: a `litmode` byte, a `dictbits`
byte, then the LSB-first bitstream. Unlike a `flags=4` LIB entry
([LIB.md § EA Compression Wrapper](LIB.md#ea-compression-wrapper-flags4)), a
`.ESA` stream carries **no** 4-byte EA decompressed-size prefix — the size is the
directory's `usize` — so it is decoded with `blast_decompress`, never
`blast_decompress_ea`. `NULL` payloads are stored verbatim.

## File Inventory

The retail `SETUP.ESA` (Disc 1, v1.00F) — 23 entries, in directory order:

| Name | Label | Flags | Method | Usize | Csize |
|------|-------|-------|--------|-------|-------|
| FA.EXE | FA_EXECUTABLE_FILES | 0x0211 | PKWA | 1,299,968 | 677,707 |
| FA.SMS | FA_EXECUTABLE_FILES | 0x0211 | PKWA | 104,452 | 50,077 |
| JANE'S HOME PAGE.URL | FA_INTERNET | 0x0211 | NULL | 49 | 49 |
| EAHELP.HLP | FA_README | 0x0211 | PKWA | 305,783 | 73,849 |
| README.TXT | FA_README | 0x0211 | PKWA | 27,816 | 10,512 |
| IP.EXE | FA_README | 0x0211 | PKWA | 708,096 | 314,488 |
| IP.CFG | FA_README | 0x0211 | NULL | 27 | 27 |
| FA_1.LIB | FA_LIBS | 0x0211 | NULL | 28,501,531 | 28,501,531 |
| FA_2.LIB | FA_LIBS | 0x0211 | NULL | 31,546,576 | 31,546,576 |
| FA_4B.LIB | FA_LIBS | 0x0211 | NULL | 34,670,738 | 34,670,738 |
| FA_4D.LIB | FA_LIBS | 0x0211 | NULL | 13,756,838 | 13,756,838 |
| CHAT.TXT | FA_MISC | 0x0211 | PKWA | 591 | 336 |
| BRIEFING.TXT | FA_MISC | 0x0211 | PKWA | 8,793 | 3,140 |
| EXAMPLE.MT | FA_MISC | 0x0211 | PKWA | 1,178 | 645 |
| LICENSE.TXT | FA_MISC | 0x0211 | PKWA | 4,672 | 2,279 |
| WAIL32.DLL | FA_SOUND_DRIVER_FILES | 0x0211 | PKWA | 135,680 | 64,347 |
| CDRVDL32.DLL | COMMDRV_DLLS_FILES | 0x0211 | PKWA | 28,672 | 12,760 |
| CDRVHF32.DLL | COMMDRV_DLLS_FILES | 0x0211 | PKWA | 29,184 | 14,121 |
| CDRVXF32.DLL | COMMDRV_DLLS_FILES | 0x0211 | PKWA | 39,424 | 20,704 |
| COMMSC32.DLL | COMMDRV_DLLS_FILES | 0x0211 | PKWA | 18,432 | 8,248 |
| EAREMOVE.EXE | REMOVER_EXECUTABLE_FILE | 0x0221 | PKWA | 325,632 | 163,684 |
| EAEXEC.EXE | EXEC_EXECUTABLE_FILE | 0x0221 | PKWA | 132,608 | 65,360 |
| PKCOMP.IDKDECODLL | SETUP_SPECIAL_FILES | 0x0211 | NULL | 19,968 | 19,968 |

Four members are also present **loose** on the Disc 1 root — `README.TXT`,
`EAHELP.HLP`, `IP.EXE` (all PKWA) and `IP.CFG` (NULL). Extracting them from the
archive reproduces the loose files byte-for-byte, which proves the codec from the
disc alone, with no installed game.

**Build note.** This is the **v1.00F** disc build. The official v1.02F patch
([fae102.exe](../reconstruction.md)) later rewrites `FA.EXE`, `FA.SMS`, `FA_1.LIB`
and `FA_2.LIB` and adds `msapi.dll`; the reconstruction database describes that
**patched** build, so a from-disc install is the earlier binary. `FA.SMS` here
declares 3,753 symbols versus 3,829 after the patch.

## Round-Trip Notes

`fx esa repack` reads the directory and rebuilds the archive: metadata is copied
verbatim, payloads are kept **stored** (still PKWA where they were PKWA), offsets
are recomputed, and the terminator is rewritten. Because the records re-encode to
the same bytes, the recomputed offsets equal the originals for a contiguous,
in-order source, so the output is byte-identical — the proof the layout is fully
accounted for. A non-contiguous or reordered source normalises instead.

`fx esa pack` builds a fresh archive with every entry **stored** (`method
"NULL"`): `fx_lib` has a PKWare DCL *decoder*, not an encoder — the same asymmetry
as [`ealib_build`](LIB.md) writing `flags=0`. It is used to synthesise fixtures,
not to re-create a shipped `SETUP.ESA`.

Both claims are checked against the retail archive by the `fa_disc_install`
integration test ([development.md § Real-media install mode](../../development.md#real-media-install-mode-fx_fa_disc1fx_fa_disc2)):
the 110 MB `SETUP.ESA` repacks byte-for-byte, and every extracted entry is hashed
against a committed manifest. The decoder also has a **self-oracle on the disc
itself** — `README.TXT`, `IP.EXE`, `IP.CFG` and `EAHELP.HLP` are shipped *both*
inside the archive and loose on disc 1, and the extracted bytes must equal the
loose ones. Three of the four are PKWA, so the DCL path is proven without
reference to anything we recorded ourselves.

## Open Questions

### Entry flags bitfield

The `flags` field is `0x0211` on app-directory entries and `0x0221` on the two
`INSTALL_SYSFILES` entries, so one bit clearly selects the destination class, but
the low bits are unexplained and the mapping is inferred from correlation rather
than read out of `SETUP.EXE`.

*Status: open — re-static (#54)*

### The malformed member name `PKCOMP.IDKDECODLL`

One entry carries a 17-character name that is not valid 8.3 and is referenced by
no `.SSF` directive; its payload is a single stored PE image, and its label
`SETUP_SPECIAL_FILES` appears nowhere in the scripts. It looks like a
name-concatenation bug in EA's archive builder (`PKCOMP.IDK` + `…DECO.DLL`?). The
codec preserves it verbatim; what consumes it is a question for the SETUP.EXE
reconstruction.

*Status: open — re-static (#54)*

## Related

**Formats:** [LIB](LIB.md) — the four `.LIB` archives ESA carries, and the DCL
contrast (LIB entries wrap the stream in a 6-byte EA header; ESA does not).
[SSF](SSF.md) — the installer script whose `INSTALL_FILES` directives select ESA
entries by `label`. [RGN](RGN.md) — the other Disc 1 installer-only format.

**Program:** [reconstruction.md](../reconstruction.md) — the SETUP.EXE
reconstruction that will confirm the flags bitfield and the `PKCOMP` member.
