#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// RTPatch — Pocket Soft .RTPatch binary-patch payload (see docs/fa/formats/RTP.md).
//
// The FA 1.02F updater (fae102.exe) carries an .RTPatch container as an overlay:
// a "K*" header, a special-files prompt table, then per-file records. Six files
// are MODIFY records (a compressed §9 opcode diff against the 1.00F original) and
// one — msapi.dll — is a NEW record (the compressed file itself). Both the diff
// and the file bytes are carried by the same custom codec: an order-0 adaptive
// Huffman over an LZSS token stream, tagged 0xB59C, MSB-first.
//
// This closes the gap between what `fx install` writes (the 1.00F discs) and what
// the reconstruction database describes (the patched 1.02F build).
//
// The 0xB59C codec, the §9 opcode grammar, and the §10 rolling checksum were
// reverse-engineered by the MIT-licensed rtptool project (github.com/bwrsandman/
// rtptool, © Sandy Carter); this is a clean-room C++ port of those facts.
//
// All functions return empty / false on malformed input and never throw, so the
// whole decode path fuzzes. Nothing here does file I/O.

namespace fx {

enum class RtpMode { Modify, New };

// One file record in the container.
struct RtpRecord {
    std::string name;            // target filename (from the record's entry)
    RtpMode     mode = RtpMode::Modify;
    uint32_t    src_size = 0;    // expected 1.00F source size (0 for New)
    uint32_t    dst_size = 0;    // 1.02F output size
    uint32_t    src_w1   = 0;    // §10 rolling checksum (31-bit) of the source
    uint32_t    src_w2   = 0;    // §10 rolling checksum (30-bit) of the source
    size_t      block_off = 0;   // offset of the 0xB59C compressed block in the payload
    uint32_t    payload_len = 0; // compressed block length (0 = runs to next record)
    int         src_count = 1;   // number of source files the record references
};

struct RtpPatch {
    uint16_t                version = 0;
    uint16_t                flags   = 0;
    bool                    extra_mode = false;
    // (name, prompt) pairs for files the installer relocates to a system dir.
    std::vector<std::pair<std::string, std::string>> specials;
    std::vector<RtpRecord>  records;
};

// Parse the container header + records from a whole .rtp payload (data starting
// at the "K*" magic — carve the overlay out of the patch .exe first). Records is
// empty on malformed input.
RtpPatch rtp_read(const uint8_t* data, size_t size);

// Decompress a 0xB59C block at block_off. For New the result is the file bytes;
// for Modify it is the §9 opcode stream. `hint` bounds the output — the stream
// self-terminates on its END marker, so any value >= the true output works.
// Empty on error.
std::vector<uint8_t> rtp_decompress(const uint8_t* data, size_t size,
                                    size_t block_off, size_t hint);

// Apply a §9 opcode stream against a source file → destination bytes. Empty on
// error (unknown opcode, truncated stream, template out of range).
std::vector<uint8_t> rtp_apply(const uint8_t* src, size_t src_size,
                               const uint8_t* ops, size_t ops_size,
                               size_t dst_size, int src_count = 1);

// §10 rolling checksum. bits = 31 for w1, 30 for w2. Both start at 0 and fold
// each byte as w = rotl8(w ^ c) within `bits` bits.
uint32_t rtp_checksum(const uint8_t* data, size_t size, int bits);

// Reconstruct one record's 1.02F output from its 1.00F source. For New, src may
// be null. When verify is true the source is checked against the record's §10
// checksums first and a mismatch returns {} — pass false to force (the caller
// then owns the risk of a wrong source). Empty on any error.
std::vector<uint8_t> rtp_reconstruct(const uint8_t* data, size_t size,
                                     const RtpRecord& rec,
                                     const uint8_t* src, size_t src_size,
                                     bool verify = true);

} // namespace fx
