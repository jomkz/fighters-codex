#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// XMI — Miles Sound System Extended MIDI (see XMI.md): an IFF-based envelope
// (FORM/XDIR + INFO + CAT XMID + per-sequence FORM XMID { TIMB, EVNT }). The
// EVNT stream is Standard-MIDI status bytes with two AIL differences: delays
// are a sum-of-bytes VLQ (each byte < 0x80 accumulates), and note-on events
// carry an explicit duration (SMF VLQ) instead of a matching note-off.
//
// This exports each sequence to a Standard MIDI File (SMF, format 0): the
// delay encoding is rewritten to SMF VLQ and each note-on's duration becomes
// a scheduled note-off. XMI→MID is a one-way translation, not a byte-identity
// round-trip.

namespace fx {

struct XmiChunk {
    std::string tag;    // "TIMB", "EVNT", ...
    uint32_t    offset; // byte offset of the chunk's data in the file
    uint32_t    size;   // data size (excludes the 8-byte tag+size header)
};

struct XmiSequence {
    std::vector<XmiChunk> chunks;   // TIMB, EVNT, ... for this sequence
    uint16_t              timbres;  // TIMB entry count (0 if absent)
};

struct XmiFile {
    bool                     valid;      // FORM..XDIR envelope present
    uint16_t                 seq_count;  // from the INFO chunk
    std::vector<XmiSequence> sequences;  // one per FORM XMID
};

// Parse the IFF structure. valid is false if the FORM/XDIR envelope is
// missing; a malformed interior yields whatever sequences parsed cleanly.
XmiFile xmi_parse(const uint8_t* data, size_t size);

// Convert one sequence to a Standard MIDI File (format 0). Returns the SMF
// bytes, or an empty vector if seq_index is out of range or the sequence has
// no EVNT chunk. ppqn is the SMF division written to the header; the XMI tick
// basis maps 1:1 (default 60, matching XMI's tempo-independent tick).
std::vector<uint8_t> xmi_to_smf(const uint8_t* data, size_t size,
                                size_t seq_index, uint16_t ppqn = 60);

} // namespace fx
