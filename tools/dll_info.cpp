// dll_info — FA overlay DLL PE analyzer
//
// Usage:
//   dll_info <file>                 auto-detect format and analyze
//   dll_info <file> --dump-data     also hex-dump the .data section
//   dll_info <file> --dump-text     also hex-dump the .text section
//
// Accepts raw (decompressed) PE DLL files extracted from a FA LIB archive.
// Use: ft lib unpack <FA_N.LIB> <outdir>  to extract first.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------

template <typename T>
static T ru(const uint8_t* p, size_t off = 0) {
    T v{};
    memcpy(&v, p + off, sizeof v);
    return v;
}

static std::vector<uint8_t> read_file(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto sz = (size_t)f.tellg();
    f.seekg(0);
    std::vector<uint8_t> buf(sz);
    f.read(reinterpret_cast<char*>(buf.data()), sz);
    return buf;
}

// ---------------------------------------------------------------------------
// PE structures
// ---------------------------------------------------------------------------

struct Section {
    char     name[9];   // null-terminated
    uint32_t vaddr;     // RVA
    uint32_t vsize;
    uint32_t raw_off;
    uint32_t raw_size;
    uint32_t charact;
    const uint8_t* data = nullptr;
};

struct ImportSlot {
    std::string dll;
    std::string func;
    uint32_t    iat_va; // VA in image (image_base + slot RVA)
};

struct PEImage {
    const uint8_t* raw  = nullptr;
    size_t         size = 0;

    uint32_t image_base = 0;
    uint32_t ep_va      = 0;  // entry-point VA (0 if none)

    std::vector<Section>             sections;
    std::map<uint32_t, ImportSlot>   by_iat_va;   // iat_va → slot
    std::map<std::string, ImportSlot> by_name;    // func name → slot
    std::map<std::string, uint32_t>  exports;     // name → VA

    bool parse();

    // Convert RVA to file offset; returns 0 on failure.
    size_t rva_to_off(uint32_t rva) const {
        for (auto& s : sections)
            if (rva >= s.vaddr && rva < s.vaddr + s.raw_size)
                return s.raw_off + (rva - s.vaddr);
        return 0;
    }

    const Section* find_section(const char* nm) const {
        for (auto& s : sections)
            if (strcmp(s.name, nm) == 0) return &s;
        return nullptr;
    }

    // Find code section by characteristics (executable + readable) or name.
    const Section* code_section() const {
        if (auto* s = find_section(".text")) return s;
        for (auto& s : sections)
            if ((s.charact & 0x20000020) == 0x20000020) return &s; // CODE|EXECUTE
        return nullptr;
    }

    // Find writable data section by characteristics or name.
    const Section* data_section() const {
        if (auto* s = find_section(".data")) return s;
        for (auto& s : sections)
            if ((s.charact & 0xC0000040) == 0xC0000040) return &s; // INITIALIZED|READ|WRITE
        return nullptr;
    }
};

bool PEImage::parse() {
    if (size < 64) return false;

    // DOS header
    if (ru<uint16_t>(raw, 0) != 0x5A4D) return false;  // "MZ"
    uint32_t pe_off = ru<uint32_t>(raw, 0x3C);
    if (pe_off + 24 > size) return false;

    // PE signature — FA overlay DLLs use "PL\0\0" (Phar Lap) instead of "PE\0\0".
    // FA has its own loader; Windows LoadLibrary never sees these files directly.
    uint32_t sig = ru<uint32_t>(raw, pe_off);
    if (sig != 0x00004550 && sig != 0x00004C50) return false;  // PE or PL

    // COFF header (immediately after PE signature, 20 bytes)
    const uint8_t* coff = raw + pe_off + 4;
    uint16_t machine        = ru<uint16_t>(coff, 0);
    uint16_t num_sections   = ru<uint16_t>(coff, 2);
    uint16_t opt_hdr_size   = ru<uint16_t>(coff, 16);
    (void)machine;

    // Optional header (PE32, magic 0x010B)
    const uint8_t* opt = coff + 20;
    if (pe_off + 24 + opt_hdr_size > size) return false;
    if (ru<uint16_t>(opt, 0) != 0x010B) return false;  // PE32

    image_base = ru<uint32_t>(opt, 28);
    ep_va      = image_base + ru<uint32_t>(opt, 16);

    // Data directories (start at opt+96 for PE32)
    // [0]=export [1]=import
    uint32_t export_rva  = ru<uint32_t>(opt, 96);
    uint32_t import_rva  = ru<uint32_t>(opt, 104);

    // Section table
    const uint8_t* sec_tbl = opt + opt_hdr_size;
    for (uint16_t i = 0; i < num_sections; i++) {
        const uint8_t* s = sec_tbl + i * 40;
        Section sec{};
        memcpy(sec.name, s, 8);
        sec.name[8]    = '\0';
        sec.vsize      = ru<uint32_t>(s, 8);
        sec.vaddr      = ru<uint32_t>(s, 12);
        sec.raw_size   = ru<uint32_t>(s, 16);
        sec.raw_off    = ru<uint32_t>(s, 20);
        sec.charact    = ru<uint32_t>(s, 36);
        if (sec.raw_off + sec.raw_size <= size)
            sec.data = raw + sec.raw_off;
        sections.push_back(sec);
    }

    // Imports
    if (import_rva) {
        size_t imp_off = rva_to_off(import_rva);
        while (imp_off && imp_off + 20 <= size) {
            uint32_t int_rva  = ru<uint32_t>(raw, imp_off);      // INT (OriginalFirstThunk)
            uint32_t name_rva = ru<uint32_t>(raw, imp_off + 12); // DLL name RVA
            uint32_t iat_rva  = ru<uint32_t>(raw, imp_off + 16); // IAT (FirstThunk)
            if (!int_rva && !name_rva && !iat_rva) break;        // terminator

            std::string dll_name;
            if (size_t noff = rva_to_off(name_rva))
                dll_name = reinterpret_cast<const char*>(raw + noff);

            // Walk INT/IAT in parallel
            size_t int_off = rva_to_off(int_rva ? int_rva : iat_rva);
            size_t iat_off = rva_to_off(iat_rva);
            uint32_t slot  = 0;
            while (int_off && int_off + 4 <= size) {
                uint32_t entry = ru<uint32_t>(raw, int_off);
                if (!entry) break;
                uint32_t iat_va = image_base + iat_rva + slot * 4;
                ImportSlot is;
                is.dll    = dll_name;
                is.iat_va = iat_va;
                if (entry & 0x80000000u) {
                    // Ordinal import
                    is.func = "#" + std::to_string(entry & 0xFFFF);
                } else {
                    // Named import: IMAGE_IMPORT_BY_NAME = {u16 hint, char name[]}
                    if (size_t fn_off = rva_to_off(entry))
                        is.func = reinterpret_cast<const char*>(raw + fn_off + 2);
                }
                by_iat_va[iat_va] = is;
                by_name[is.func]  = is;
                int_off += 4;
                iat_off += 4;
                slot++;
            }
            imp_off += 20;
        }
    }

    // Exports
    if (export_rva) {
        size_t exp_off = rva_to_off(export_rva);
        if (exp_off + 40 <= size) {
            uint32_t num_names   = ru<uint32_t>(raw, exp_off + 24);
            uint32_t names_rva   = ru<uint32_t>(raw, exp_off + 32);
            uint32_t addrs_rva   = ru<uint32_t>(raw, exp_off + 28);
            uint32_t ords_rva    = ru<uint32_t>(raw, exp_off + 36);
            for (uint32_t i = 0; i < num_names; i++) {
                size_t name_ptr_off = rva_to_off(names_rva + i * 4);
                size_t ord_off      = rva_to_off(ords_rva  + i * 2);
                if (!name_ptr_off || !ord_off) continue;
                uint32_t name_rva2 = ru<uint32_t>(raw, name_ptr_off);
                uint16_t ord       = ru<uint16_t>(raw, ord_off);
                size_t fn_name_off = rva_to_off(name_rva2);
                size_t fn_addr_off = rva_to_off(addrs_rva + ord * 4);
                if (!fn_name_off || !fn_addr_off) continue;
                std::string name = reinterpret_cast<const char*>(raw + fn_name_off);
                uint32_t    fn_va = image_base + ru<uint32_t>(raw, fn_addr_off);
                exports[name] = fn_va;
            }
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// Thunk + dispatch-table analysis
//
// FA overlay DLLs use Phar Lap PE format.  Imports are NOT called via
// FF 15 [IAT] — instead a small JMP thunk table lives at the end of the
// CODE section:
//
//   FF 25 [iat_va LE]    ; JMP DWORD PTR [IAT slot]
//
// The dialog/HUD logic is implemented as a DISPATCH TABLE, not compiled x86
// code.  Each record in the table begins with a 4-byte thunk VA, followed
// immediately by coordinate and string-pointer parameters.
//
// Record layout (empirically from CHOOSEAC.DLG):
//   u32  thunk_va    – VA of the JMP thunk (identifies the draw function)
//   u16  x           – screen x (pixels)
//   u16  y           – screen y (pixels)
//   u8   unknown[4]  – zeros / padding
//   u16  (unknown)
//   u16  width       – button/control width in pixels
//   u32  str_va      – VA of null-terminated label string (within CODE section)
// ---------------------------------------------------------------------------

struct CallSite {
    uint32_t va;       // VA of the record (= thunk_va field location)
    size_t   file_off;
    int16_t  x, y;     // screen coordinates decoded from record
    int16_t  width;
    uint32_t str_va;   // VA of label string (0 if not present)
    std::string label; // resolved label (empty if not resolved)
    std::vector<int32_t> args; // raw s16 param values following the thunk VA
};

// Build thunk_va → import_name map by scanning for FF 25 [iat_va] in code.
static std::map<uint32_t, std::string> find_thunks(const PEImage& pe) {
    std::map<uint32_t, std::string> thunks; // thunk_va → func_name

    const Section* code = pe.code_section();
    if (!code || !code->data) return thunks;

    for (auto& [iat_va, slot] : pe.by_iat_va) {
        uint8_t needle[6] = {0xFF, 0x25};
        memcpy(needle + 2, &iat_va, 4);

        for (size_t i = 0; i + 6 <= code->raw_size; i++) {
            if (memcmp(code->data + i, needle, 6) == 0) {
                uint32_t thunk_va = pe.image_base + code->vaddr + (uint32_t)i;
                thunks[thunk_va] = slot.func;
            }
        }
    }
    return thunks;
}

// Resolve a string VA to a C string within the PE image.
static std::string resolve_string(const PEImage& pe, uint32_t str_va) {
    if (!str_va || str_va < pe.image_base) return {};
    size_t off = pe.rva_to_off(str_va - pe.image_base);
    if (!off || off >= pe.size) return {};
    // Read up to 128 chars
    const char* p = reinterpret_cast<const char*>(pe.raw + off);
    size_t max = std::min(pe.size - off, (size_t)128);
    size_t n = strnlen(p, max);
    if (!n) return {};
    return std::string(p, n);
}

// Scan all sections for occurrences of thunk_va as a u32, then decode the
// record that follows (x, y, padding, width, str_va).
static std::vector<CallSite> find_dispatch_entries(const PEImage& pe,
                                                    uint32_t thunk_va,
                                                    const std::string& func_name) {
    std::vector<CallSite> sites;

    // Search every section with data
    for (auto& sec : pe.sections) {
        if (!sec.data || sec.raw_size < 4) continue;

        uint8_t needle[4];
        memcpy(needle, &thunk_va, 4);

        for (size_t i = 0; i + 4 <= sec.raw_size; i++) {
            if (memcmp(sec.data + i, needle, 4) != 0) continue;

            CallSite cs{};
            cs.file_off = sec.raw_off + i;
            cs.va       = pe.image_base + sec.vaddr + (uint32_t)i;

            // Read up to 16 bytes of parameters after the thunk VA.
            // Empirical layout: x(2) y(2) unk(4) unk2(2) w(2) str_ptr(4)
            const uint8_t* p = sec.data + i + 4;
            size_t remain    = sec.raw_size - i - 4;

            if (remain >= 2) cs.x = (int16_t)ru<uint16_t>(p, 0);
            if (remain >= 4) cs.y = (int16_t)ru<uint16_t>(p, 2);
            // Empirical layout (CHOOSEAC.DLG): x(2) y(2) pad(10) width(2) str_va(4)
            if (remain >= 16) cs.width  = (int16_t)ru<uint16_t>(p, 14);
            if (remain >= 20) {
                cs.str_va = ru<uint32_t>(p, 16);
                cs.label  = resolve_string(pe, cs.str_va);
            }

            // Also collect all s16 values in next 16 bytes as raw args
            for (size_t j = 0; j + 2 <= std::min(remain, (size_t)16); j += 2)
                cs.args.push_back((int32_t)(int16_t)ru<uint16_t>(p, j));

            sites.push_back(cs);
        }
    }
    return sites;
}

// find_call_sites: try dispatch-table approach first; fall back to FF 15 CALL scan.
static std::vector<CallSite> find_call_sites(const PEImage& pe,
                                             const std::string& func_name) {
    auto thunks = find_thunks(pe);

    // Try thunk-based dispatch table
    for (auto& [thunk_va, name] : thunks) {
        if (name == func_name) {
            auto sites = find_dispatch_entries(pe, thunk_va, func_name);
            if (!sites.empty()) return sites;
        }
    }

    // Fallback: classic FF 15 [IAT] CALL pattern
    auto it = pe.by_name.find(func_name);
    if (it == pe.by_name.end()) return {};
    uint32_t iat_va = it->second.iat_va;

    const Section* text = pe.code_section();
    if (!text || !text->data) return {};

    uint8_t needle[6] = {0xFF, 0x15};
    memcpy(needle + 2, &iat_va, 4);

    std::vector<CallSite> sites;
    for (size_t i = 0; i + 6 <= text->raw_size; i++) {
        if (memcmp(text->data + i, needle, 6) != 0) continue;
        CallSite cs{};
        cs.file_off = text->raw_off + i;
        cs.va       = pe.image_base + text->vaddr + (uint32_t)i;

        // Collect PUSH immediates in the 128 bytes before this CALL (right-to-left, reversed).
        std::vector<int32_t> rtl;
        size_t wstart = i > 128 ? i - 128 : 0;
        size_t pos = wstart;
        while (pos < i) {
            uint8_t b = text->data[pos];
            if (b == 0x6A && pos + 2 <= i) {
                rtl.push_back((int32_t)(int8_t)text->data[pos + 1]);
                pos += 2;
            } else if (b == 0x68 && pos + 5 <= i) {
                int32_t v; memcpy(&v, text->data + pos + 1, 4);
                rtl.push_back(v);
                pos += 5;
            } else {
                pos++;
            }
        }
        std::reverse(rtl.begin(), rtl.end());
        cs.args = rtl;
        sites.push_back(cs);
    }
    return sites;
}

// ---------------------------------------------------------------------------
// Coordinate-table scanner for .data section
// ---------------------------------------------------------------------------

struct CoordRun {
    size_t offset;
    std::vector<std::pair<uint16_t, uint16_t>> pairs;
};

// Find runs of at least min_pairs consecutive (x,y) word pairs where
// x in [0,x_max] and y in [0,y_max].
static std::vector<CoordRun> scan_coord_runs(const uint8_t* data, size_t size,
                                              uint16_t x_max = 640,
                                              uint16_t y_max = 480,
                                              size_t min_pairs = 2) {
    std::vector<CoordRun> runs;
    CoordRun cur;
    cur.offset = 0;

    for (size_t i = 0; i + 4 <= size; i += 4) {
        uint16_t x = ru<uint16_t>(data, i);
        uint16_t y = ru<uint16_t>(data, i + 2);
        if (x <= x_max && y <= y_max && (x || y)) {
            if (cur.pairs.empty()) cur.offset = i;
            cur.pairs.emplace_back(x, y);
        } else {
            if (cur.pairs.size() >= min_pairs) runs.push_back(cur);
            cur.pairs.clear();
        }
    }
    if (cur.pairs.size() >= min_pairs) runs.push_back(cur);
    return runs;
}

// ---------------------------------------------------------------------------
// Hex dump helper
// ---------------------------------------------------------------------------

static void hex_dump(const uint8_t* data, size_t size, uint32_t base_va) {
    for (size_t i = 0; i < size; i += 16) {
        printf("  %08X  ", base_va + (uint32_t)i);
        for (size_t j = 0; j < 16; j++) {
            if (i + j < size) printf("%02X ", data[i + j]);
            else               printf("   ");
            if (j == 7) printf(" ");
        }
        printf(" |");
        for (size_t j = 0; j < 16 && i + j < size; j++) {
            uint8_t c = data[i + j];
            printf("%c", (c >= 0x20 && c < 0x7F) ? c : '.');
        }
        printf("|\n");
    }
}

// ---------------------------------------------------------------------------
// Format detection
// ---------------------------------------------------------------------------

static std::string detect_format(const PEImage& pe, size_t file_size) {
    // Export-based detection
    if (pe.exports.count("_T_HorizonProc")) return "LAY";

    // Import-based detection
    bool has_draw_action = pe.by_name.count("_DrawAction") > 0;
    bool has_draw_text   = pe.by_name.count("_DrawText")   > 0;
    if (has_draw_action || has_draw_text) return "DLG";

    // No imports: distinguish HUD vs MUS by CODE section first byte.
    // MUS: starts with 0xFF (playlist opcode).
    // HUD: starts with 0x00 followed by a '~'-prefixed sprite name.
    const Section* code = pe.code_section();
    if (code && code->data && code->raw_size >= 2) {
        if (code->data[0] == 0xFF) return "MUS";
        if (code->data[0] == 0x00 && code->data[1] == 0x7E) return "HUD";
    }

    // Size-based fallback
    if (file_size == 16896) return "LAY";
    if (file_size == 8704 || file_size == 12800) return "FNT";

    return "unknown";
}

// ---------------------------------------------------------------------------
// Format-specific analysis
// ---------------------------------------------------------------------------

static void analyze_dlg(const PEImage& pe) {
    static const char* draw_funcs[] = {
        "_DrawAction", "_DrawRocker", "_DrawEditBox",
        "_DrawText", "_DrawFormattedText", "_DrawCampaignList", nullptr
    };

    printf("\n--- DLG control analysis ---\n");
    for (int i = 0; draw_funcs[i]; i++) {
        auto sites = find_call_sites(pe, draw_funcs[i]);
        if (sites.empty()) continue;
        printf("\n%s  (%zu call site%s)\n", draw_funcs[i],
               sites.size(), sites.size() == 1 ? "" : "s");
        for (auto& cs : sites) {
            printf("  VA=%08X  x=%-4d y=%-4d w=%-4d",
                   cs.va, cs.x, cs.y, cs.width);
            if (!cs.label.empty())
                printf("  \"%s\"", cs.label.c_str());
            else if (cs.str_va)
                printf("  str_va=%08X (unresolved)", cs.str_va);
            printf("\n");
        }
    }

    // Also scan data section for coordinate tables
    const Section* data_sec = pe.data_section();
    if (!data_sec) data_sec = pe.find_section("$$DOSX");  // Phar Lap data segment
    if (data_sec && data_sec->data) {
        auto runs = scan_coord_runs(data_sec->data, data_sec->raw_size);
        if (!runs.empty()) {
            printf("\n%s coordinate runs:\n", data_sec->name);
            for (auto& r : runs) {
                printf("  offset +%04zX: ", r.offset);
                for (auto& [x, y] : r.pairs) printf("(%u,%u) ", x, y);
                printf("\n");
            }
        }
    }
}

static void analyze_hud(const PEImage& pe) {
    printf("\n--- HUD analysis ---\n");

    // HUD CODE section layout:
    //   null-padded 14-byte records of sprite names (~aircraft_xxx)
    //   followed by a "hud" marker string and coordinate/parameter blocks.
    // No imports — the engine loads sprites by name.

    const Section* code = pe.code_section();
    if (!code || !code->data) { printf("no CODE section\n"); return; }

    // Collect all null-terminated strings in CODE (min 2 chars, printable ASCII).
    printf("Strings in CODE section:\n");
    const uint8_t* d = code->data;
    size_t sz = code->raw_size;
    for (size_t i = 0; i < sz; ) {
        if (d[i] == 0) { i++; continue; }
        // Check if printable run is long enough to be meaningful
        size_t j = i;
        while (j < sz && d[j] >= 0x20 && d[j] < 0x7F) j++;
        if (j - i >= 2 && (j >= sz || d[j] == 0)) {
            std::string s(reinterpret_cast<const char*>(d + i), j - i);
            printf("  VA=%08X  \"%s\"\n",
                   (uint32_t)(pe.image_base + code->vaddr + i), s.c_str());
        }
        i = j + 1;
    }

    // Scan for signed 16-bit coordinate-like pairs (values in [-640, 640]).
    printf("\nSigned s16 pairs in CODE (possible gauge offsets, |val|<=640):\n");
    bool found_any = false;
    for (size_t i = 0; i + 4 <= sz; i += 2) {
        int16_t a = (int16_t)ru<uint16_t>(d, i);
        int16_t b = (int16_t)ru<uint16_t>(d, i + 2);
        if (a != 0 && b != 0 && std::abs(a) <= 640 && std::abs(b) <= 640) {
            printf("  VA=%08X  (%d, %d)\n",
                   (uint32_t)(pe.image_base + code->vaddr + i), a, b);
            found_any = true;
            i += 2; // skip b, advance past this pair
        }
    }
    if (!found_any) printf("  (none)\n");
}

static void analyze_mus(const PEImage& pe) {
    printf("\n--- MUS playlist analysis ---\n");

    // MUS CODE section is a bytecode script:
    //   FF <name\0>        — playlist identifier string
    //   FA <sub> <u32>     — 6-byte setup opcode (volume, tempo, etc.)
    //   FB 50 <idx> F9     — play XMI track <idx>
    //   FE <u32>           — 5-byte conditional/jump
    //   FD <u24>           — 4-byte loop/branch
    //   01..03 <...>       — inline sub-blocks (epilogue marker)

    const Section* code = pe.code_section();
    if (!code || !code->data) { printf("no CODE section\n"); return; }

    const uint8_t* d = code->data;
    size_t sz = code->raw_size;
    size_t i = 0;

    // Playlist name: FF <string\0>
    if (i < sz && d[i] == 0xFF) {
        i++;
        const char* name = reinterpret_cast<const char*>(d + i);
        printf("Playlist: \"%s\"\n", name);
        while (i < sz && d[i]) i++;
        i++; // skip null
    }

    // Walk opcodes
    printf("\nOpcodes:\n");
    std::vector<int> xmi_tracks;
    while (i < sz && d[i] >= 0xFA) {
        uint8_t op = d[i];
        if (op == 0xFA && i + 5 < sz) {
            uint8_t sub = d[i+1];
            uint32_t arg = ru<uint32_t>(d, i+2);
            printf("  FA %02X  arg=%08X\n", sub, arg);
            i += 6;
        } else if (op == 0xFB && i + 3 < sz && d[i+1] == 0x50) {
            uint8_t idx = d[i+2];
            printf("  FB 50  XMI track %3u  (next=%02X)\n", idx, d[i+3]);
            xmi_tracks.push_back(idx);
            i += 4; // FB 50 idx F9
        } else if (op == 0xFE && i + 4 < sz) {
            uint32_t arg = ru<uint32_t>(d, i+1);
            printf("  FE     arg=%08X\n", arg);
            i += 5;
        } else if (op == 0xFD && i + 3 < sz) {
            uint32_t arg = (uint32_t)d[i+1] | ((uint32_t)d[i+2] << 8) | ((uint32_t)d[i+3] << 16);
            printf("  FD     arg=%06X\n", arg);
            i += 4;
        } else {
            printf("  %02X  (unknown at offset +%04zX)\n", op, i);
            i++;
        }
    }

    if (!xmi_tracks.empty()) {
        printf("\nXMI track sequence (%zu tracks): ", xmi_tracks.size());
        for (int t : xmi_tracks) printf("%d ", t);
        printf("\n");
    }
}

static void analyze_fnt(const PEImage& pe, size_t file_size) {
    printf("\n--- FNT bitmap analysis ---\n");

    const Section* data_sec = pe.data_section();
    if (!data_sec) data_sec = pe.find_section("$$DOSX");
    const Section* rdata    = pe.find_section(".rdata");

    // Summarise data sections
    for (auto* s : {data_sec, rdata}) {
        if (!s) continue;
        printf("\n%s  size=%u  raw_off=0x%X\n", s->name, s->raw_size, s->raw_off);

        // Scan for long runs of non-zero bytes (candidate bitmap data)
        size_t run = 0, best_run = 0, best_off = 0;
        for (size_t i = 0; i < s->raw_size; i++) {
            if (s->data[i]) { if (!run) best_off = i; run++; }
            else             { if (run > best_run) { best_run = run; } run = 0; }
        }
        printf("  Longest non-zero run: %zu bytes at offset +%04zX\n", best_run, best_off);

        // Count non-zero bytes
        size_t nz = 0;
        for (size_t i = 0; i < s->raw_size; i++) if (s->data[i]) nz++;
        printf("  Non-zero bytes: %zu / %u  (%.1f%%)\n",
               nz, s->raw_size, 100.0 * nz / s->raw_size);

        // Look for a small word that could be a glyph count or stride
        printf("  First 32 u16 values:");
        for (size_t i = 0; i + 2 <= std::min((size_t)s->raw_size, (size_t)64); i += 2)
            printf(" %u", ru<uint16_t>(s->data, i));
        printf("\n");
    }

    // Size-based glyph hypothesis
    printf("\nFile size: %zu bytes\n", file_size);
    for (int bpp : {1, 4, 8}) {
        for (int w : {4, 5, 6, 8}) {
            for (int h : {6, 8, 10, 12, 14, 16}) {
                int bytes_per_glyph = (w * h * bpp + 7) / 8;
                if (bytes_per_glyph == 0) continue;
                // Try common glyph counts
                for (int n : {96, 128, 224, 256}) {
                    // Approximate: data_size ≈ file_size - 0x600 (PE headers)
                    int data_approx = (int)file_size - 0x600;
                    if (data_approx <= 0) continue;
                    int residual = data_approx % bytes_per_glyph;
                    if (residual < 16) { // close match
                        printf("  Hypothesis: %d×%d %d-bpp × %d glyphs = %d bytes"
                               " (residual %d)\n",
                               w, h, bpp, n, bytes_per_glyph * n, residual);
                    }
                }
            }
        }
    }
}

static void analyze_lay(const PEImage& pe) {
    printf("\n--- LAY atmosphere analysis ---\n");

    // _T_HorizonProc call sites (calls it makes into the engine)
    printf("Exported: _T_HorizonProc VA=%08X\n",
           pe.exports.count("_T_HorizonProc") ? pe.exports.at("_T_HorizonProc") : 0);

    // Scan .data for gradient characteristics
    const Section* data_sec = pe.data_section();
    if (!data_sec) data_sec = pe.find_section("$$DOSX");
    if (!data_sec || !data_sec->data) { printf("no data section\n"); return; }
    printf("\n%s  size=%u bytes  (%.1f KB)\n", data_sec->name,
           data_sec->raw_size, data_sec->raw_size / 1024.0);

    // Find runs that look like smooth gradient ramps (monotonic byte sequences)
    struct GradRun { size_t off, len; bool ascending; };
    std::vector<GradRun> grads;
    size_t i = 0;
    while (i + 1 < data_sec->raw_size) {
        int8_t dir = (int8_t)(data_sec->data[i+1]) - (int8_t)(data_sec->data[i]);
        if (dir == 0) { i++; continue; }
        bool asc = dir > 0;
        size_t start = i;
        while (i + 1 < data_sec->raw_size) {
            int8_t d2 = (int8_t)(data_sec->data[i+1]) - (int8_t)(data_sec->data[i]);
            if ((asc && d2 > 0) || (!asc && d2 < 0)) i++;
            else break;
        }
        size_t run_len = i - start + 1;
        if (run_len >= 16)
            grads.push_back({start, run_len, asc});
        i++;
    }

    printf("\nMonotonic gradient runs (≥16 bytes):\n");
    if (grads.empty()) {
        printf("  (none)\n");
    } else {
        // Sort by length, show top 10
        std::sort(grads.begin(), grads.end(),
                  [](const GradRun& a, const GradRun& b){ return a.len > b.len; });
        for (size_t k = 0; k < std::min(grads.size(), (size_t)10); k++) {
            auto& g = grads[k];
            printf("  offset +%05zX  len=%4zu  %s  "
                   "range [%3u..%3u]  first bytes: ",
                   g.off, g.len, g.ascending ? "asc " : "desc",
                   data_sec->data[g.off],
                   data_sec->data[g.off + g.len - 1]);
            for (size_t j = 0; j < std::min(g.len, (size_t)8); j++)
                printf("%02X ", data_sec->data[g.off + j]);
            printf("\n");
        }
    }

    // Imported engine functions
    printf("\nImported functions:\n");
    for (auto& [va, slot] : pe.by_iat_va)
        printf("  %s!%s  IAT VA=%08X\n", slot.dll.c_str(), slot.func.c_str(), va);
}

// ---------------------------------------------------------------------------
// Main report
// ---------------------------------------------------------------------------

static void print_pe_info(const PEImage& pe, size_t file_size) {
    printf("Image base:    0x%08X\n", pe.image_base);
    printf("Entry point:   0x%08X\n", pe.ep_va);
    printf("File size:     %zu bytes\n", file_size);

    printf("\nSections:\n");
    for (auto& s : pe.sections) {
        printf("  %-8s  VA=0x%08X  vsize=0x%04X  raw_off=0x%05X  raw_size=0x%04X\n",
               s.name, pe.image_base + s.vaddr, s.vsize, s.raw_off, s.raw_size);
    }

    if (!pe.exports.empty()) {
        printf("\nExports:\n");
        for (auto& [name, va] : pe.exports)
            printf("  %-40s  VA=0x%08X\n", name.c_str(), va);
    }

    if (!pe.by_iat_va.empty()) {
        std::string cur_dll;
        printf("\nImports:\n");
        for (auto& [va, slot] : pe.by_iat_va) {
            if (slot.dll != cur_dll) {
                printf("  [%s]\n", slot.dll.c_str());
                cur_dll = slot.dll;
            }
            printf("    %-40s  IAT VA=0x%08X\n", slot.func.c_str(), va);
        }
    }
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: dll_info <file.DLL> [--dump-data] [--dump-text]\n");
        return 1;
    }

    bool dump_data = false, dump_text = false;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--dump-data") == 0) dump_data = true;
        if (strcmp(argv[i], "--dump-text") == 0) dump_text = true;
    }

    auto data = read_file(argv[1]);
    if (data.empty()) {
        fprintf(stderr, "Cannot read: %s\n", argv[1]);
        return 1;
    }

    PEImage pe;
    pe.raw  = data.data();
    pe.size = data.size();
    if (!pe.parse()) {
        fprintf(stderr, "Not a valid PE32 file: %s\n", argv[1]);
        return 1;
    }

    printf("=== %s ===\n", argv[1]);
    print_pe_info(pe, data.size());

    std::string fmt = detect_format(pe, data.size());
    printf("\nDetected format: %s\n", fmt.c_str());

    if      (fmt == "DLG") analyze_dlg(pe);
    else if (fmt == "HUD") analyze_hud(pe);
    else if (fmt == "MUS") analyze_mus(pe);
    else if (fmt == "LAY") analyze_lay(pe);
    else if (fmt == "FNT") analyze_fnt(pe, data.size());

    if (dump_data) {
        const Section* s = pe.data_section();
        if (!s) s = pe.find_section("$$DOSX");
        if (s && s->data) {
            printf("\n--- %s hex dump ---\n", s->name);
            hex_dump(s->data, s->raw_size, pe.image_base + s->vaddr);
        }
    }
    if (dump_text) {
        const Section* s = pe.code_section();
        if (s && s->data) {
            printf("\n--- %s hex dump ---\n", s->name);
            hex_dump(s->data, s->raw_size, pe.image_base + s->vaddr);
        }
    }

    return 0;
}
