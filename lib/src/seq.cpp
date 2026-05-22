#include "fx/seq.h"
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace fx {

// ---- tokenizer -------------------------------------------------------

// Split a raw event body (everything after leading TAB) into tokens,
// respecting "quoted strings" as single tokens.
static std::vector<std::string> tokenize(const std::string& s) {
    std::vector<std::string> tokens;
    size_t i = 0;
    while (i < s.size()) {
        // skip whitespace (spaces and tabs)
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
        if (i >= s.size()) break;

        if (s[i] == '"') {
            // quoted string
            ++i;
            std::string tok;
            while (i < s.size() && s[i] != '"') tok += s[i++];
            if (i < s.size()) ++i; // consume closing "
            tokens.push_back('"' + tok + '"');
        } else {
            std::string tok;
            while (i < s.size() && s[i] != ' ' && s[i] != '\t') tok += s[i++];
            tokens.push_back(tok);
        }
    }
    return tokens;
}

// Parse the time token: optional '+', then decimal integer.
static bool parse_time(const std::string& tok, bool& relative, int& ticks) {
    if (tok.empty()) return false;
    size_t start = 0;
    relative = false;
    if (tok[0] == '+') { relative = true; start = 1; }
    else if (tok[0] == '-') { start = 0; } // negative ticks (rare but possible)
    // Check that the rest is digits (or sign+digits)
    bool any_digit = false;
    for (size_t i = start; i < tok.size(); ++i) {
        if (!std::isdigit((unsigned char)tok[i])) return false;
        any_digit = true;
    }
    if (!any_digit) return false;
    ticks = std::atoi(tok.c_str() + (relative ? 1 : 0));
    return true;
}

// ---- parse -----------------------------------------------------------

SeqFile seq_parse(const uint8_t* data, size_t size) {
    SeqFile result;

    // Split into lines (handle CRLF and LF).
    // Bytes after the last CRLF that don't form a terminated line are kept as trailer.
    std::vector<std::string> raw_lines;
    std::string trailer;
    {
        std::string cur;
        bool had_terminator = false;
        for (size_t i = 0; i < size; ++i) {
            char c = (char)data[i];
            if (c == '\r') {
                if (i + 1 < size && data[i + 1] == '\n') ++i;
                raw_lines.push_back(cur);
                cur.clear();
                had_terminator = true;
            } else if (c == '\n') {
                raw_lines.push_back(cur);
                cur.clear();
                had_terminator = true;
            } else {
                cur += c;
            }
        }
        // Bytes after the last line terminator become the trailer.
        // If the file has no terminator at all, treat the whole thing as one line.
        if (!cur.empty()) {
            if (had_terminator) {
                trailer = cur; // e.g. a lone 0x1A DOS-EOF byte
            } else {
                raw_lines.push_back(cur);
            }
        }
    }

    for (auto& line : raw_lines) {
        result.lines.push_back(line);

        // Event lines start with a TAB character
        bool is_ev = !line.empty() && line[0] == '\t';
        result.is_event.push_back(is_ev);

        if (!is_ev) {
            // comment or blank -- store a placeholder event so indices stay aligned
            result.events.push_back(SeqEvent{});
            result.events.back().raw = line;
            continue;
        }

        // Strip the leading tab; keep the rest as raw
        std::string body = line.substr(1);

        SeqEvent ev;
        ev.raw = body;

        auto tokens = tokenize(body);
        if (!tokens.empty()) {
            // First token: time
            size_t ti = 0;
            if (parse_time(tokens[ti], ev.relative, ev.ticks)) {
                ++ti;
            }
            // Optional 'sync' keyword before command
            if (ti < tokens.size() && tokens[ti] == "sync") {
                ev.sync = true;
                ++ti;
            }
            // Command
            if (ti < tokens.size()) {
                ev.command = tokens[ti++];
            }
            // Args
            for (; ti < tokens.size(); ++ti) {
                ev.args.push_back(tokens[ti]);
            }
        }

        result.events.push_back(ev);
    }

    result.trailer = trailer;
    return result;
}

// ---- serialize -------------------------------------------------------

std::vector<uint8_t> seq_serialize(const SeqFile& seq) {
    std::vector<uint8_t> out;

    for (size_t i = 0; i < seq.lines.size(); ++i) {
        bool is_ev = (i < seq.is_event.size()) && seq.is_event[i];
        const std::string& line = seq.lines[i];

        if (is_ev) {
            // Re-emit with leading TAB + raw body
            out.push_back('\t');
            const std::string& raw = (i < seq.events.size()) ? seq.events[i].raw : line.substr(1);
            for (char c : raw) out.push_back((uint8_t)c);
        } else {
            for (char c : line) out.push_back((uint8_t)c);
        }
        out.push_back('\r');
        out.push_back('\n');
    }

    // Preserve any trailing bytes (e.g. DOS EOF 0x1A) without adding CRLF.
    for (char c : seq.trailer) out.push_back((uint8_t)c);

    return out;
}

} // namespace fx
