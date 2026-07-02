#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// EALIB archive format.
// Magic "EALIB" + uint16 count + 18-byte entries (13-byte name, 1-byte flags, uint32 offset).
// File sizes are implicit: size[i] = offset[i+1] - offset[i]; last = EOF - offset[last].
//
// Flags:
//   0 = raw/uncompressed
//   1 = LZSS (4-byte decompressed size prefix) -- not yet implemented
//   3 = PxPk (raw + inline PXPK header) -- not yet implemented
//   4 = PKWare DCL with 4-byte EA decompressed-size prefix

namespace fx {

struct Entry {
    char     name[13]; // null-terminated 8.3 filename
    uint8_t  flags;
    uint32_t offset;
    uint32_t size;     // compressed/raw size in the LIB
};

// Parse the directory of a memory-mapped LIB. Returns empty vector on error.
std::vector<Entry> ealib_read_dir(const uint8_t* data, size_t size);

// Find a directory entry by name (ASCII case-insensitive).
// Returns nullptr if not found.
const Entry* ealib_find(const std::vector<Entry>& entries, const std::string& name);

// Extract one entry's data.
// If decompress=true and flags==4, runs blast decompression automatically.
// Returns empty vector on error.
std::vector<uint8_t> ealib_extract(const uint8_t* lib_data, size_t lib_size,
                                    const Entry& entry, bool decompress = true);

// Build a new EALIB from a list of (filename, data) pairs.
// All entries are stored as flags=0 (raw).
std::vector<uint8_t> ealib_build(
    const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files);

// Patch one named file into an existing LIB.
// The replacement is stored as flags=0 (raw). All other entries are preserved.
std::vector<uint8_t> ealib_patch(const uint8_t* lib_data, size_t lib_size,
                                  const std::string& name,
                                  const std::vector<uint8_t>& new_data);

} // namespace fx
