#include "fx/ealib.h"
#include "fx/blast.h"
#include <algorithm>
#include <cctype>
#include <cstring>

namespace fx {

static const char MAGIC[]  = "EALIB";
static const int  MAGIC_LEN = 5;
static const int  HDR_BASE  = 7;   // 5 magic + 2 count
static const int  ENTRY_SZ  = 18;  // 13 name + 1 flags + 4 offset

static uint16_t read_u16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}
static uint32_t read_u32(const uint8_t* p) {
    return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}
static void write_u16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
}
static void write_u32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

std::vector<Entry> ealib_read_dir(const uint8_t* data, size_t size) {
    if (size < (size_t)(HDR_BASE + ENTRY_SZ)) return {};
    if (memcmp(data, MAGIC, MAGIC_LEN) != 0) return {};

    uint16_t count = read_u16(data + 5);
    size_t dir_end = HDR_BASE + (size_t)count * ENTRY_SZ;
    if (dir_end > size) return {};

    std::vector<Entry> entries(count);
    for (int i = 0; i < (int)count; i++) {
        const uint8_t* e = data + HDR_BASE + i * ENTRY_SZ;
        memcpy(entries[i].name, e, 13);
        entries[i].name[12] = '\0';
        entries[i].flags  = e[13];
        entries[i].offset = read_u32(e + 14);
    }

    // Compute sizes from adjacent offsets
    for (int i = 0; i < (int)count - 1; i++) {
        uint32_t next = entries[i + 1].offset;
        uint32_t cur  = entries[i].offset;
        entries[i].size = (next > cur) ? (next - cur) : 0;
    }
    if (count > 0) {
        uint32_t last_off = entries[count - 1].offset;
        entries[count - 1].size = (size > last_off) ? (uint32_t)(size - last_off) : 0;
    }

    return entries;
}

const Entry* ealib_find(const std::vector<Entry>& entries, const std::string& name) {
    for (const Entry& e : entries) {
        const char* a = e.name;
        const char* b = name.c_str();
        while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) {
            ++a;
            ++b;
        }
        if (*a == '\0' && *b == '\0') return &e;
    }
    return nullptr;
}

std::vector<uint8_t> ealib_extract(const uint8_t* lib_data, size_t lib_size,
                                    const Entry& entry, bool decompress) {
    if (entry.offset + entry.size > lib_size) return {};
    const uint8_t* src = lib_data + entry.offset;
    size_t src_size = entry.size;

    if (!decompress || entry.flags != 4) {
        return std::vector<uint8_t>(src, src + src_size);
    }

    // flags==4: EA-wrapped PKWare DCL
    if (src_size < 4) return {};
    uint32_t expected = read_u32(src);
    std::vector<uint8_t> out(expected);
    int got = blast_decompress_ea(src, src_size, out.data(), expected);
    if (got < 0) return {};
    out.resize((size_t)got);
    return out;
}

std::vector<uint8_t> ealib_build(
    const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files) {

    uint16_t count = (uint16_t)files.size();
    size_t dir_size = HDR_BASE + (size_t)count * ENTRY_SZ;

    size_t data_size = 0;
    for (auto& f : files) data_size += f.second.size();

    std::vector<uint8_t> out(dir_size + data_size, 0);

    memcpy(out.data(), MAGIC, MAGIC_LEN);
    write_u16(out.data() + 5, count);

    uint32_t offset = (uint32_t)dir_size;
    for (int i = 0; i < (int)count; i++) {
        uint8_t* e = out.data() + HDR_BASE + i * ENTRY_SZ;
        const std::string& name = files[i].first;
        size_t nlen = std::min(name.size(), (size_t)12);
        memcpy(e, name.c_str(), nlen);
        e[13] = 0; // flags=0 (raw)
        write_u32(e + 14, offset);
        offset += (uint32_t)files[i].second.size();
    }

    size_t pos = dir_size;
    for (auto& f : files) {
        memcpy(out.data() + pos, f.second.data(), f.second.size());
        pos += f.second.size();
    }

    return out;
}

std::vector<uint8_t> ealib_patch(const uint8_t* lib_data, size_t lib_size,
                                  const std::string& name,
                                  const std::vector<uint8_t>& new_data) {
    auto entries = ealib_read_dir(lib_data, lib_size);
    if (entries.empty()) return {};

    std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
    files.reserve(entries.size());

    for (auto& e : entries) {
        std::string ename(e.name);
        if (ename == name) {
            files.push_back({ ename, new_data });
        } else {
            auto data = ealib_extract(lib_data, lib_size, e, true);
            files.push_back({ ename, std::move(data) });
        }
    }

    return ealib_build(files);
}

} // namespace fx
