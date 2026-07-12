#include "fx/esa.h"
#include "fx/blast.h"
#include <cctype>
#include <cstring>

namespace fx {

static const char MAGIC[]   = "ELECTRONIC_ARTS_ARCHIVE_FILE";
static const size_t MAGIC_LEN = 28;                 // chars, excluding the NUL
static const size_t MAGIC_SZ  = MAGIC_LEN + 1;      // on disk: 28 chars + NUL

// PKWA decompressed-size ceiling. `usize` comes from the directory, but a crafted
// archive can claim anything; the largest real FA-era payload is a few MiB (#168).
static const uint32_t MAX_DECOMP = 64u << 20; // 64 MiB

// A defensive cap on the directory: no real ESA is anywhere near this, and it
// stops a hostile file from spinning even though the cursor already bounds it.
static const size_t MAX_ENTRIES = 100000;

static uint32_t read_u32(const uint8_t* p) {
    return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}
static void put_u32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back((uint8_t)(x));
    v.push_back((uint8_t)(x >> 8));
    v.push_back((uint8_t)(x >> 16));
    v.push_back((uint8_t)(x >> 24));
}
static void put_cstr(std::vector<uint8_t>& v, const std::string& s) {
    v.insert(v.end(), s.begin(), s.end());
    v.push_back(0);
}

// Read a NUL-terminated string starting at *pos, bounded by size. Advances *pos
// past the NUL. Returns false (malformed) if no NUL is found within the buffer.
static bool read_cstr(const uint8_t* data, size_t size, size_t* pos, std::string* out) {
    size_t p = *pos;
    while (p < size && data[p] != 0) p++;
    if (p >= size) return false;            // ran off the end with no terminator
    out->assign((const char*)data + *pos, p - *pos);
    *pos = p + 1;
    return true;
}

// Parse every directory record. On success *dir_end is the byte after the
// terminator (== the first blob offset). Returns false on any malformed input.
static bool parse_dir(const uint8_t* data, size_t size, uint64_t archive_size,
                      std::vector<EsaEntry>* out, size_t* dir_end) {
    if (size < MAGIC_SZ) return false;
    if (memcmp(data, MAGIC, MAGIC_LEN) != 0 || data[MAGIC_LEN] != 0) return false;

    std::vector<EsaEntry> entries;
    size_t p = MAGIC_SZ;
    for (;;) {
        if (p >= size) return false;        // no terminator before EOF
        if (data[p] == 0) { p++; break; }   // empty name = directory terminator
        if (entries.size() >= MAX_ENTRIES) return false;

        EsaEntry e;
        if (!read_cstr(data, size, &p, &e.name))  return false;
        if (!read_cstr(data, size, &p, &e.label)) return false;
        if (p + 12 > size) return false;
        e.flags = read_u32(data + p);     p += 4;
        e.usize = read_u32(data + p);     p += 4;
        e.mtime = read_u32(data + p);     p += 4;
        if (!read_cstr(data, size, &p, &e.method)) return false;
        if (p + 8 > size) return false;
        e.csize  = read_u32(data + p);    p += 4;
        e.offset = read_u32(data + p);    p += 4;

        if (e.method != "PKWA" && e.method != "NULL") return false;
        if (e.method == "NULL" && e.csize != e.usize) return false;
        entries.push_back(std::move(e));
    }

    // The blobs sit after the directory, contiguous. Validate each offset lies
    // in [dir_end, archive_size] with no 32-bit wrap. dir_end == p here.
    for (const EsaEntry& e : entries) {
        if (e.offset < p) return false;                        // overlaps the directory
        if ((uint64_t)e.offset + e.csize > archive_size) return false;  // past EOF
    }
    *out = std::move(entries);
    *dir_end = p;
    return true;
}

std::vector<EsaEntry> esa_read_dir(const uint8_t* data, size_t size, uint64_t archive_size) {
    if (archive_size == 0) archive_size = size;
    std::vector<EsaEntry> entries;
    size_t dir_end = 0;
    if (!parse_dir(data, size, archive_size, &entries, &dir_end)) return {};
    return entries;
}

size_t esa_dir_size(const uint8_t* data, size_t size) {
    std::vector<EsaEntry> entries;
    size_t dir_end = 0;
    if (!parse_dir(data, size, size, &entries, &dir_end)) return 0;
    return dir_end;
}

const EsaEntry* esa_find(const std::vector<EsaEntry>& entries, const std::string& name) {
    for (const EsaEntry& e : entries) {
        if (e.name.size() != name.size()) continue;
        bool eq = true;
        for (size_t i = 0; i < name.size(); i++) {
            if (std::tolower((unsigned char)e.name[i]) !=
                std::tolower((unsigned char)name[i])) { eq = false; break; }
        }
        if (eq) return &e;
    }
    return nullptr;
}

std::string esa_safe_name(const std::string& name) {
    // "." and ".." would escape or alias the output directory.
    if (name == "." || name == "..") return "_";
    std::string out = name;
    for (char& c : out) {
        unsigned char u = (unsigned char)c;
        if (u < 0x20 || c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }
    return out;
}

std::vector<uint8_t> esa_extract(const uint8_t* data, size_t size,
                                 const EsaEntry& entry, bool decompress,
                                 bool* unsupported) {
    if (unsupported) *unsupported = false;
    if ((uint64_t)entry.offset + entry.csize > size) return {};
    const uint8_t* src = data + entry.offset;

    if (!decompress) return std::vector<uint8_t>(src, src + entry.csize);

    if (entry.method == "NULL") {
        return std::vector<uint8_t>(src, src + entry.csize);
    }
    if (entry.method == "PKWA") {
        if (entry.usize > MAX_DECOMP) return {};
        std::vector<uint8_t> out(entry.usize);
        int n = blast_decompress(src, entry.csize, out.data(), out.size());
        // The directory pins the size exactly, so a short/long decode is
        // corruption — stricter than ealib_extract, which tolerates a short claim.
        if (n < 0 || (size_t)n != entry.usize) return {};
        return out;
    }
    if (unsupported) *unsupported = true;
    return {};
}

// Emit one directory record. Offsets are assigned by the caller.
static void emit_record(std::vector<uint8_t>& v, const EsaEntry& e) {
    put_cstr(v, e.name);
    put_cstr(v, e.label);
    put_u32(v, e.flags);
    put_u32(v, e.usize);
    put_u32(v, e.mtime);
    put_cstr(v, e.method);
    put_u32(v, e.csize);
    put_u32(v, e.offset);
}

// The encoded size of a record is independent of its offset value (always 4B).
static size_t record_size(const EsaEntry& e) {
    return e.name.size() + 1 + e.label.size() + 1 + 4 + 4 + 4
         + e.method.size() + 1 + 4 + 4;
}

std::vector<uint8_t> esa_repack(const uint8_t* data, size_t size) {
    std::vector<EsaEntry> entries;
    size_t dir_end = 0;
    if (!parse_dir(data, size, size, &entries, &dir_end)) return {};

    // The directory re-encodes to the same length (records are byte-for-byte the
    // same shape), so recomputed offsets equal the originals for a contiguous,
    // in-order source — hence byte-identical. A non-contiguous source normalises.
    size_t new_dir = MAGIC_SZ + 1;                 // magic + terminator
    for (const EsaEntry& e : entries) new_dir += record_size(e);

    uint32_t cursor = (uint32_t)new_dir;
    for (EsaEntry& e : entries) { e.offset = cursor; cursor += e.csize; }

    std::vector<uint8_t> out;
    out.reserve(cursor);
    out.insert(out.end(), (const uint8_t*)MAGIC, (const uint8_t*)MAGIC + MAGIC_SZ);
    for (const EsaEntry& e : entries) emit_record(out, e);
    out.push_back(0);                              // terminator
    // Payloads, copied verbatim (kept stored) from the source in directory order.
    std::vector<EsaEntry> src_dir;
    parse_dir(data, size, size, &src_dir, &dir_end);
    for (const EsaEntry& e : src_dir)
        out.insert(out.end(), data + e.offset, data + e.offset + e.csize);
    return out;
}

std::vector<uint8_t> esa_build(const std::vector<EsaInput>& files) {
    std::vector<EsaEntry> entries;
    entries.reserve(files.size());
    for (const EsaInput& f : files) {
        EsaEntry e;
        e.name  = f.name;
        e.label = f.label;
        e.flags = f.flags;
        e.usize = (uint32_t)f.data.size();
        e.mtime = f.mtime;
        e.method = "NULL";                         // stored: no DCL encoder
        e.csize = (uint32_t)f.data.size();
        entries.push_back(std::move(e));
    }

    size_t dir = MAGIC_SZ + 1;
    for (const EsaEntry& e : entries) dir += record_size(e);
    uint32_t cursor = (uint32_t)dir;
    for (EsaEntry& e : entries) { e.offset = cursor; cursor += e.csize; }

    std::vector<uint8_t> out;
    out.reserve(cursor);
    out.insert(out.end(), (const uint8_t*)MAGIC, (const uint8_t*)MAGIC + MAGIC_SZ);
    for (const EsaEntry& e : entries) emit_record(out, e);
    out.push_back(0);
    for (const EsaInput& f : files)
        out.insert(out.end(), f.data.begin(), f.data.end());
    return out;
}

} // namespace fx
