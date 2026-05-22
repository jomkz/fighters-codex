#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// .SEQ -- FA cutscene / sequence timeline format.
//
// Plain ASCII text. Lines starting with ';' or blank = comments.
// Event lines start with a TAB:
//   <TAB><time> [sync] <command> [args...]<CRLF>
//
// time: decimal int ticks; '+' prefix = relative to previous event.
// sync: optional keyword between time and command.
// command: palette, font, fadein, fadeout, video, sound, bitmap, wait, sync.
// args:    space-separated; quoted strings use "..." syntax.
//
// Round-trip: parse -> serialize produces byte-for-byte identical output
// when the source uses standard CRLF line endings.

namespace fx {

struct SeqEvent {
    // Raw line text (everything after the leading TAB, before CRLF).
    // Stored verbatim for perfect round-trip fidelity.
    std::string raw;

    // Parsed fields (populated by seq_parse; empty/zero if unparseable).
    bool        relative = false; // true when time token starts with '+'
    int         ticks    = 0;
    bool        sync     = false; // 'sync' keyword present before command
    std::string command;          // first non-time-non-sync token
    std::vector<std::string> args; // remaining tokens (quoted strings preserved)
};

struct SeqFile {
    // All lines in document order.  Each element is the raw line text
    // including any leading whitespace, WITHOUT the terminating CRLF.
    // Event lines have a leading TAB; comment/blank lines do not.
    // SeqEvent metadata corresponds 1-to-1 with lines[] entries.
    std::vector<std::string> lines;
    std::vector<bool>        is_event; // parallel to lines[]
    std::vector<SeqEvent>    events;   // parallel to lines[] (non-events have empty command)

    // Bytes that follow the last CRLF (e.g. the DOS EOF marker 0x1A).
    // Serialized verbatim at the end of the output, without a trailing CRLF.
    std::string trailer;
};

// Parse a .SEQ file from raw bytes. Returns empty SeqFile on error.
SeqFile seq_parse(const uint8_t* data, size_t size);

// Serialize back to bytes (CRLF line endings).
std::vector<uint8_t> seq_serialize(const SeqFile& seq);

} // namespace fx
