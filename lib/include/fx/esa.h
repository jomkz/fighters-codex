#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// ESA — "ELECTRONIC_ARTS_ARCHIVE_FILE" installer archive (see docs/fa/formats/ESA.md).
//
// SETUP.ESA sits in the FA Disc 1 root and is the sole source of the installed
// game files: FA.EXE, FA.SMS, FA_1/2/4B/4D.LIB, WAIL32.DLL, the comms DLLs, and
// the loose install text. The EA installer (SETUP.EXE) reads it, guided by the
// .SSF scripts, whose INSTALL_FILES directives select entries by their `label`.
//
// Layout:
//   magic  char[29]  "ELECTRONIC_ARTS_ARCHIVE_FILE\0"
//   dir    record[]  variable-length records, back to back
//   term   u8        0x00 — an empty (zero-length) name terminates the directory
//   data   blob[]    payloads, contiguous, in directory order; the first begins
//                    at the byte after the terminator, the last ends at EOF
//
// Record (all u32 little-endian; strings NUL-terminated, NOT 8.3 — names may
// hold spaces and apostrophes, e.g. "JANE'S HOME PAGE.URL"):
//   name\0  label\0  u32 flags  u32 usize  u32 mtime  method\0  u32 csize  u32 offset
//
//   flags   0x211 = app-dir file / 0x221 = INSTALL_SYSFILES (Windows system dir).
//           The low bits are inferred until the SETUP.EXE RE track confirms them.
//   method  "PKWA" = PKWare DCL, or "NULL" = stored (csize == usize).
//
// PKWA payloads are RAW PKWare DCL streams (litmode, dictbits, bitstream). Unlike
// a flags=4 LIB entry they carry NO 4-byte EA size prefix — the size is the
// directory's `usize` — so they decode with blast_decompress(), never
// blast_decompress_ea().

namespace fx {

struct EsaEntry {
    std::string name;
    std::string label;   // the token .SSF INSTALL_FILES selects on
    uint32_t    flags  = 0;
    uint32_t    usize  = 0;
    uint32_t    mtime  = 0; // unix timestamp
    std::string method;  // "PKWA" | "NULL"
    uint32_t    csize  = 0;
    uint32_t    offset = 0;
};

// Parse the directory. `data`/`size` need only cover the directory (a prefix of
// the file), so a caller may pass just the head; `archive_size` is the total
// archive length used to bounds-check blob offsets — pass 0 to mean `size`.
// Returns an empty vector on any malformed input.
std::vector<EsaEntry> esa_read_dir(const uint8_t* data, size_t size,
                                   uint64_t archive_size = 0);

// Bytes from the magic through the terminator; equals the first blob's offset in
// a well-formed archive. 0 on error.
size_t esa_dir_size(const uint8_t* data, size_t size);

// Find an entry by name (ASCII case-insensitive). nullptr if not found.
const EsaEntry* esa_find(const std::vector<EsaEntry>& entries, const std::string& name);

// A filename legal and identical on every platform: / \ : * ? " < > | and
// control characters become '_', and "." / ".." are neutralised. Spaces and
// apostrophes pass through (unlike ealib_safe_name, which also folds '&' — a
// LIB-only audio-prefix convention with no meaning here). The malformed member
// "PKCOMP.IDKDECODLL" survives verbatim.
std::string esa_safe_name(const std::string& name);

// Extract one entry. decompress=true blast-decodes PKWA into a usize buffer and
// copies NULL verbatim; an unknown method returns {} and sets *unsupported=true
// (the ealib_extract contract; *unsupported is set false on every other path).
// decompress=false returns the stored bytes for any method. Empty vector on a
// bounds/decode error.
std::vector<uint8_t> esa_extract(const uint8_t* data, size_t size,
                                 const EsaEntry& entry, bool decompress = true,
                                 bool* unsupported = nullptr);

// Rebuild the container from its own directory: metadata verbatim, payloads kept
// stored (still compressed), offsets recomputed, the terminator rewritten.
// Byte-identical to the input for well-formed archives — the proof the layout is
// understood. Returns an empty vector on error.
std::vector<uint8_t> esa_repack(const uint8_t* data, size_t size);

// Build an archive. Every entry is stored (method "NULL"): fx_lib has a DCL
// decoder, not an encoder — the same asymmetry as ealib_build writing flags=0.
struct EsaInput {
    std::string         name;
    std::string         label;
    uint32_t            flags = 0x211;
    uint32_t            mtime = 0;
    std::vector<uint8_t> data;
};
std::vector<uint8_t> esa_build(const std::vector<EsaInput>& files);

} // namespace fx
