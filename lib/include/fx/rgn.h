#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// RGN — installer UI region map (see RGN.md): u32 record count + 40-byte
// records of (char[4] null-padded name, u32 vertex count, 4 × (x, y) u32
// pairs). The count fully determines the file size (4 + 40 × count).

namespace fx {

struct RgnRecord {
    uint8_t  name[4];      // raw null-padded label bytes
    uint32_t vertex_count; // 4 in every shipped record (rectangles)
    uint32_t xy[8];        // 4 × (x, y), clockwise from top-left
};

struct RgnFile {
    std::vector<RgnRecord> records;
};

// Parse. false unless size == 4 + 40 × count exactly.
bool rgn_read(const uint8_t* data, size_t size, RgnFile& out);

// Serialize — byte-identical inverse of rgn_read.
std::vector<uint8_t> rgn_write(const RgnFile& rgn);

// Record label as a printable string (up to the first NUL).
std::string rgn_name(const RgnRecord& rec);

} // namespace fx
