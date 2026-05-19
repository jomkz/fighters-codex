#include "ft/bi.h"
#include "ft/pe.h"
#include <cstring>
#include <map>
#include <set>
#include <cstdio>

namespace ft {

static uint16_t u16le_bi(const uint8_t* p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t u32le_bi(const uint8_t* p) {
    return p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}

struct BiSection {
    char name[9];
    uint32_t vma;
    uint32_t raw_ptr;
    uint32_t raw_sz;
};

static std::vector<BiSection> parse_bi_sections(const uint8_t* data, size_t size) {
    std::vector<BiSection> secs;
    if (size < 0x40) return secs;
    uint32_t pe_off = u32le_bi(data + 0x3C);
    if (pe_off + 24 > size) return secs;
    const uint8_t* pe = data + pe_off;
    if ((pe[0] != 'P') || (pe[1] != 'L' && pe[1] != 'E') || pe[2] != 0 || pe[3] != 0)
        return secs;
    uint16_t num_sec = u16le_bi(pe + 6);
    uint16_t opt_sz  = u16le_bi(pe + 20);
    uint32_t sec_tbl = pe_off + 24 + opt_sz;
    for (uint16_t i = 0; i < num_sec; ++i) {
        uint32_t off = sec_tbl + (uint32_t)i * 40;
        if (off + 40 > size) break;
        BiSection s;
        memcpy(s.name, data + off, 8);
        s.name[8] = '\0';
        s.vma     = u32le_bi(data + off + 12);
        s.raw_sz  = u32le_bi(data + off + 16);
        s.raw_ptr = u32le_bi(data + off + 20);
        secs.push_back(s);
    }
    return secs;
}

// Build a map from CALL_DIRECT address (thunk VA) → function name.
static std::map<uint32_t, std::string> build_import_map(
    const uint8_t* data, size_t size,
    const std::vector<BiSection>& secs)
{
    std::map<uint32_t, std::string> m;

    const BiSection* code_sec  = nullptr;
    const BiSection* idata_sec = nullptr;
    for (const auto& s : secs) {
        if (strncmp(s.name, "CODE",   8) == 0) code_sec  = &s;
        if (strncmp(s.name, ".idata", 8) == 0) idata_sec = &s;
    }
    if (!code_sec || !idata_sec) return m;
    if (idata_sec->raw_ptr + idata_sec->raw_sz > size) return m;
    if (idata_sec->raw_sz < 20) return m;

    const uint8_t* idata    = data + idata_sec->raw_ptr;
    uint32_t       idata_vm = idata_sec->vma;
    uint32_t       idata_sz = idata_sec->raw_sz;

    uint32_t orig_thunk_va = u32le_bi(idata);       // INT: array of VA → hint/name
    uint32_t first_thunk_va = u32le_bi(idata + 16); // IAT base VA
    if (orig_thunk_va == 0 || first_thunk_va == 0) return m;

    auto va2idata = [&](uint32_t va) -> const uint8_t* {
        if (va < idata_vm) return nullptr;
        uint32_t off = va - idata_vm;
        if (off >= idata_sz) return nullptr;
        return idata + off;
    };

    // Walk INT to collect ordered function names
    const uint8_t* int_ptr = va2idata(orig_thunk_va);
    if (!int_ptr) return m;

    std::vector<std::string> names;
    for (;;) {
        uint32_t base_off = (uint32_t)(int_ptr - idata);
        if (base_off + 4 > idata_sz) break;
        uint32_t entry_va = u32le_bi(int_ptr);
        int_ptr += 4;
        if (entry_va == 0) break;
        // IMAGE_IMPORT_BY_NAME: 2-byte hint then name string
        const uint8_t* np = va2idata(entry_va + 2);
        if (!np) { names.push_back(""); continue; }
        std::string fname;
        while (*np && (uint32_t)(np - idata) < idata_sz)
            fname += (char)(*np++);
        names.push_back(fname);
    }

    // Scan CODE for JMP [mem] thunks: FF 25 <u32>
    if (code_sec->raw_ptr + code_sec->raw_sz > size) return m;
    const uint8_t* code = data + code_sec->raw_ptr;
    for (uint32_t i = 0; i + 6 <= code_sec->raw_sz; ++i) {
        if (code[i] == 0xFF && code[i+1] == 0x25) {
            uint32_t iat_va = u32le_bi(code + i + 2);
            if (iat_va < first_thunk_va) continue;
            uint32_t slot = (iat_va - first_thunk_va) / 4;
            if (slot < names.size() && !names[slot].empty()) {
                uint32_t thunk_va = code_sec->vma + i;
                m[thunk_va] = names[slot];
            }
        }
    }
    return m;
}

static const char* var_name_bi(uint8_t idx) {
    static const char* n[] = {"%a", "%b", "%c", "%d"};
    return idx < 4 ? n[idx] : "?";
}

std::vector<BiInstr> bi_disasm(const uint8_t* data, size_t size) {
    std::vector<BiInstr> result;

    auto secs       = parse_bi_sections(data, size);
    auto import_map = build_import_map(data, size, secs);
    CodeSection cs  = pe_code_section(data, size);
    if (!cs.data || cs.size == 0) return result;

    const uint8_t* base = cs.data;
    const uint8_t* end  = cs.data + cs.size;

    // Pre-pass: collect jump targets for label annotations
    std::set<uint32_t> targets;
    {
        const uint8_t* p = base;
        while (p < end) {
            uint8_t op = *p;
            if (op == 0x25) break;
            if (op == 0x00 && (p+1 >= end || *(p+1) == 0x00)) break;
            switch (op) {
            case 0x20: case 0x21: case 0x23:
                if (p + 3 <= end) {
                    int16_t off = (int16_t)u16le_bi(p + 1);
                    targets.insert((uint32_t)(int32_t)off);
                }
                p += 3; break;
            case 0x24:
                if (p + 2 <= end) {
                    uint8_t n = p[1];
                    for (uint8_t i = 0; i < n && p + 2 + (uint32_t)i*2 + 2 <= end; ++i) {
                        int16_t off = (int16_t)u16le_bi(p + 2 + i*2);
                        targets.insert((uint32_t)(int32_t)off);
                    }
                    p += 2 + n*2;
                } else p++;
                break;
            case 0x01: p += 5; break;
            case 0x02: p += 3; break;
            case 0x03: p += 2; break;
            case 0x05: case 0x06: p += 2; break;
            case 0x07: p++; while (p < end && *p) p++; if (p < end) p++; break;
            case 0x26: p += 5; break;
            case 0x27: p++; while (p < end && *p) p++; if (p < end) p++; break;
            case 0x28: p += 5; break;
            default: p++; break;
            }
        }
    }

    // Main disassembly pass
    const uint8_t* p = base;
    while (p < end) {
        uint8_t  op     = *p;
        uint32_t offset = (uint32_t)(p - base);

        // Label annotation
        if (targets.count(offset)) {
            char lbl[32];
            snprintf(lbl, sizeof(lbl), "@%04X:", offset);
            result.push_back({offset, lbl});
        }

        if (op == 0x25) { result.push_back({offset, "END"}); break; }
        if (op == 0x00) {
            if (p + 1 < end && *(p+1) == 0x00) break;
            result.push_back({offset, "NOP"}); p++; continue;
        }

        char buf[512];
        std::string text;

        switch (op) {
        case 0x01: {
            int32_t v = (int32_t)u32le_bi(p+1);
            if ((uint32_t)v == 0x7FFFFFFF) text = "PUSH_DWORD 0x7FFFFFFF (any)";
            else { snprintf(buf,sizeof(buf),"PUSH_DWORD %d",v); text=buf; }
            p += 5; break;
        }
        case 0x02: {
            int16_t v = (int16_t)u16le_bi(p+1);
            snprintf(buf,sizeof(buf),"PUSH_WORD %d",(int)v); text=buf; p+=3; break;
        }
        case 0x03: {
            int8_t v = (int8_t)p[1];
            snprintf(buf,sizeof(buf),"PUSH_BYTE %d",(int)v); text=buf; p+=2; break;
        }
        case 0x04: text="EVAL"; p++; break;
        case 0x05:
            snprintf(buf,sizeof(buf),"STORE_VAR %s",var_name_bi(p[1]));
            text=buf; p+=2; break;
        case 0x06:
            snprintf(buf,sizeof(buf),"LOAD_VAR %s",var_name_bi(p[1]));
            text=buf; p+=2; break;
        case 0x07: {
            p++;
            std::string name;
            while (p < end && *p) name += (char)(*p++);
            if (p < end) p++;
            snprintf(buf,sizeof(buf),"PUSH_ADDR \"%s\"",name.c_str());
            text=buf; break;
        }
        case 0x08: text="MUL";  p++; break;
        case 0x09: text="DIV";  p++; break;
        case 0x0A: text="MOD";  p++; break;
        case 0x0B: text="ADD";  p++; break;
        case 0x0C: text="SUB";  p++; break;
        case 0x0D: text="AND";  p++; break;
        case 0x0E: text="OR";   p++; break;
        case 0x0F: text="XOR";  p++; break;
        case 0x10: text="SHL";  p++; break;
        case 0x11: text="SHR";  p++; break;
        case 0x12: text="LT";   p++; break;
        case 0x13: text="LE";   p++; break;
        case 0x14: text="GE";   p++; break;
        case 0x15: text="GT";   p++; break;
        case 0x16: text="EQ";   p++; break;
        case 0x17: text="NE";   p++; break;
        case 0x18: text="LAND"; p++; break;
        case 0x19: text="LOR";  p++; break;
        case 0x1A: text="ABS";  p++; break;
        case 0x1B: text="NEG";  p++; break;
        case 0x1C: text="NOT";  p++; break;
        case 0x1D: text="RANDOM";  p++; break;
        case 0x1E: text="PERCENT"; p++; break;
        case 0x1F: text="CHANCE";  p++; break;
        case 0x20: {
            int16_t off = (int16_t)u16le_bi(p+1);
            snprintf(buf,sizeof(buf),"GOTO @%04X",(unsigned)(uint32_t)(int32_t)off);
            text=buf; p+=3; break;
        }
        case 0x21: {
            int16_t off = (int16_t)u16le_bi(p+1);
            snprintf(buf,sizeof(buf),"PUSH_GOTO @%04X",(unsigned)(uint32_t)(int32_t)off);
            text=buf; p+=3; break;
        }
        case 0x22: text="JUMP"; p++; break;
        case 0x23: {
            int16_t off = (int16_t)u16le_bi(p+1);
            snprintf(buf,sizeof(buf),"IF_FALSE @%04X",(unsigned)(uint32_t)(int32_t)off);
            text=buf; p+=3; break;
        }
        case 0x24: {
            uint8_t n = p[1];
            std::string s;
            snprintf(buf,sizeof(buf),"SWITCH %d",(int)n); s=buf;
            for (uint8_t i = 0; i < n && p + 2 + (uint32_t)i*2 + 2 <= end; ++i) {
                int16_t off = (int16_t)u16le_bi(p + 2 + i*2);
                snprintf(buf,sizeof(buf)," @%04X",(unsigned)(uint32_t)(int32_t)off); s+=buf;
            }
            text=s; p += 2 + n*2; break;
        }
        case 0x26: {
            uint32_t va = u32le_bi(p+1);
            auto it = import_map.find(va);
            if (it != import_map.end())
                snprintf(buf,sizeof(buf),"CALL_DIRECT \"%s\"",it->second.c_str());
            else
                snprintf(buf,sizeof(buf),"CALL_DIRECT 0x%08X",va);
            text=buf; p+=5; break;
        }
        case 0x27: {
            p++;
            std::string name;
            while (p < end && *p) name += (char)(*p++);
            if (p < end) p++;
            snprintf(buf,sizeof(buf),"CALL_BY_NAME \"%s\"",name.c_str());
            text=buf; break;
        }
        case 0x28: {
            int16_t v1 = (int16_t)u16le_bi(p+1);
            int16_t v2 = (int16_t)u16le_bi(p+3);
            snprintf(buf,sizeof(buf),"FRAME %d, %d",(int)v1,(int)v2);
            text=buf; p+=5; break;
        }
        default: {
            snprintf(buf,sizeof(buf),"?? 0x%02X",op); text=buf; p++; break;
        }
        }

        result.push_back({offset, text});
    }

    return result;
}

} // namespace ft
