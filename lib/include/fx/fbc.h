#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

// FBC — frame-size index paired with a RATVID .VDO (see FBC.md).
// No magic or header: a flat array of N u32le values, one per video frame,
// each the byte size of that frame's data in the paired .VDO. Frame n's
// payload starts at 816 (the VDO header size) + sum(sizes[0..n)).

namespace fx {

// Parse an FBC. File size must be a multiple of 4; *ok reports validity
// (an empty vector is a legal, zero-frame index).
std::vector<uint32_t> fbc_read(const uint8_t* data, size_t size,
                               bool* ok = nullptr);

// Serialize — the byte-identical inverse of fbc_read.
std::vector<uint8_t> fbc_write(const std::vector<uint32_t>& frame_sizes);

// Byte offset of frame n's data inside the paired .VDO.
// n may equal frame_sizes.size(): that yields the expected VDO file size.
uint64_t fbc_frame_offset(const std::vector<uint32_t>& frame_sizes, size_t n);

} // namespace fx
