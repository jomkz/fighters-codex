#include "fx/pe.h"

namespace fx {

static uint16_t u16le(const uint8_t* p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t u32le(const uint8_t* p) {
    return (uint32_t)(p[0] | ((uint32_t)p[1] << 8) |
                      ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

CodeSection pe_code_section(const uint8_t* data, size_t size) {
    if (size < 0x40 || data[0] != 'M' || data[1] != 'Z')
        return {nullptr, 0, 0};
    uint32_t pe_off = u32le(data + 0x3C);
    // 64-bit sums throughout: every offset here is attacker-controlled, and
    // 32-bit arithmetic wraps past the bounds checks (found by fuzz_pe).
    if ((uint64_t)pe_off + 24 + 2 > size)
        return {nullptr, 0, 0};
    const uint8_t* pe = data + pe_off;
    if (pe[0] != 'P' || pe[1] != 'L' || pe[2] != 0 || pe[3] != 0)
        return {nullptr, 0, 0};
    uint16_t num_sec    = u16le(pe + 6);
    uint16_t opt_hdr_sz = u16le(pe + 20);
    uint64_t sec_table  = (uint64_t)pe_off + 24 + opt_hdr_sz;
    for (uint16_t i = 0; i < num_sec; ++i) {
        uint64_t sec_off = sec_table + (uint64_t)i * 40;
        if (sec_off + 40 > size) break;
        const uint8_t* sec = data + sec_off;
        uint32_t virt_addr = u32le(sec + 12);  // section VirtualAddress
        uint32_t raw_sz    = u32le(sec + 16);
        uint32_t raw_ptr   = u32le(sec + 20);
        if (raw_sz > 0 && (uint64_t)raw_ptr + raw_sz <= size)
            return {data + raw_ptr, raw_sz, virt_addr};
    }
    return {nullptr, 0, 0};
}

// ---- import table ----------------------------------------------------

namespace {

// A section table entry, as needed to map an RVA back to a file offset.
struct Section {
    uint32_t va, vsize, raw_ptr, raw_size;
};

// Every value below is read from the file. 64-bit sums throughout, and each mapped range is
// checked against `size` -- the same discipline pe_code_section learned from fuzz_pe.
size_t rva_to_offset(const std::vector<Section>& secs, uint32_t rva, size_t size) {
    for (const Section& s : secs) {
        uint64_t span = s.vsize > s.raw_size ? s.vsize : s.raw_size;
        if (rva < s.va || rva >= (uint64_t)s.va + span) continue;
        uint64_t off = (uint64_t)s.raw_ptr + (rva - s.va);
        return (off < size) ? (size_t)off : (size_t)-1;
    }
    return (size_t)-1;
}

// A NUL-terminated string starting at `off`, bounded by the buffer.
std::string cstr_at(const uint8_t* data, size_t size, size_t off, size_t max_len = 256) {
    if (off == (size_t)-1 || off >= size) return {};
    std::string s;
    for (size_t i = off; i < size && s.size() < max_len; ++i) {
        if (data[i] == 0) return s;
        s += (char)data[i];
    }
    return {};  // unterminated -- treat as malformed rather than guess
}

}  // namespace

std::vector<PeImport> pe_imports(const uint8_t* data, size_t size) {
    std::vector<PeImport> out;
    if (size < 0x40 || data[0] != 'M' || data[1] != 'Z') return out;

    uint32_t pe_off = u32le(data + 0x3C);
    if ((uint64_t)pe_off + 24 > size) return out;
    const uint8_t* pe = data + pe_off;
    if (pe[0] != 'P' || pe[1] != 'L' || pe[2] != 0 || pe[3] != 0) return out;

    const uint16_t num_sec    = u16le(pe + 6);
    const uint16_t opt_hdr_sz = u16le(pe + 20);
    const uint64_t opt        = (uint64_t)pe_off + 24;

    // The optional header must hold PE32 magic and reach data directory 1 (imports), which
    // sits at offset 96 + 8*1 within it.
    if (opt + 112 > size || opt_hdr_sz < 112) return out;
    if (u16le(data + opt) != 0x10B) return out;   // PE32
    const uint32_t num_dirs = u32le(data + opt + 92);
    if (num_dirs < 2) return out;
    const uint32_t imp_rva = u32le(data + opt + 96 + 8);
    if (imp_rva == 0) return out;

    std::vector<Section> secs;
    const uint64_t sec_table = opt + opt_hdr_sz;
    for (uint16_t i = 0; i < num_sec; ++i) {
        uint64_t so = sec_table + (uint64_t)i * 40;
        if (so + 40 > size) break;
        const uint8_t* s = data + so;
        secs.push_back({u32le(s + 12), u32le(s + 8), u32le(s + 20), u32le(s + 16)});
    }

    // Import descriptors: 20 bytes each, terminated by an all-zero entry.
    size_t d = rva_to_offset(secs, imp_rva, size);
    for (int guard = 0; guard < 64; ++guard) {   // no shipped overlay imports 64 modules
        if (d == (size_t)-1 || d + 20 > size) break;
        const uint32_t ilt  = u32le(data + d);
        const uint32_t name = u32le(data + d + 12);
        const uint32_t iat  = u32le(data + d + 16);
        if (name == 0) break;

        const std::string module = cstr_at(data, size, rva_to_offset(secs, name, size));
        // The lookup table is the authority; the IAT is its runtime twin and stands in when
        // the linker omitted the ILT.
        size_t t = rva_to_offset(secs, ilt ? ilt : iat, size);
        for (int n = 0; n < 4096; ++n) {
            if (t == (size_t)-1 || t + 4 > size) break;
            const uint32_t entry = u32le(data + t);
            if (entry == 0) break;
            PeImport imp;
            imp.module = module;
            if (entry & 0x80000000u) {
                imp.ordinal = (uint16_t)(entry & 0xFFFF);   // imported by ordinal
            } else {
                // A hint/name entry: u16 hint, then the NUL-terminated name.
                size_t ho = rva_to_offset(secs, entry, size);
                if (ho == (size_t)-1 || ho + 2 >= size) break;
                imp.name = cstr_at(data, size, ho + 2);
                if (imp.name.empty()) break;
            }
            out.push_back(imp);
            t += 4;
        }
        d += 20;
    }
    return out;
}

} // namespace fx
