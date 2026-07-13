#include "fx/brf.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace fx {

// ---- helpers ----------------------------------------------------------

static std::string strip_leading(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    return s.substr(i);
}

static bool starts_with(const std::string& s, const char* prefix) {
    return s.rfind(prefix, 0) == 0;
}

// Parse a quoted string: "hello world" -> hello world.
// Returns the content between the first pair of double quotes.
static std::string unquote(const std::string& s) {
    size_t a = s.find('"');
    if (a == std::string::npos) return s;
    size_t b = s.find('"', a + 1);
    if (b == std::string::npos) return s.substr(a + 1);
    return s.substr(a + 1, b - a - 1);
}

// Split a stripped line into first word + rest.
static std::pair<std::string,std::string> split_first(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && s[i] != ' ' && s[i] != '\t') ++i;
    std::string first = s.substr(0, i);
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    return { first, s.substr(i) };
}

// ---- parse ------------------------------------------------------------

BrfDoc brf_parse(const uint8_t* data, size_t size) {
    BrfDoc doc;

    // Split into lines (CRLF and LF both accepted)
    std::vector<std::string> lines;
    {
        std::string cur;
        for (size_t i = 0; i < size; ++i) {
            char c = (char)data[i];
            if (c == '\r') {
                if (i + 1 < size && data[i + 1] == '\n') ++i;
                lines.push_back(cur); cur.clear();
            } else if (c == '\n') {
                lines.push_back(cur); cur.clear();
            } else if (c != '\x1A') { // skip DOS EOF marker
                cur += c;
            }
        }
        if (!cur.empty()) lines.push_back(cur);
    }

    doc.raw_lines = lines;

    // State machine: scan for data fields and pointer tables
    bool in_table = false;
    std::string current_table_name;
    std::vector<std::string> current_table_strings;

    // The file names its own records: ";---- START OF PLANE_TYPE ----". Track that, and the
    // byte offset within it, so every field carries where it lives -- read from the file
    // rather than assumed from a table.
    std::string current_section;
    uint32_t    section_offset = 0;

    for (auto& line : lines) {
        std::string stripped = strip_leading(line);

        // Empty line or comment: skip -- but a comment may DELIMIT A RECORD, which is a fact.
        if (!stripped.empty() && stripped[0] == ';') {
            size_t s0 = stripped.find("START OF ");
            size_t e0 = stripped.find("END OF ");
            if (s0 != std::string::npos) {
                std::string n = stripped.substr(s0 + 9);
                while (!n.empty() && (n.back() == '-' || n.back() == ' ')) n.pop_back();
                current_section = n;
                section_offset  = 0;
            } else if (e0 != std::string::npos) {
                current_section.clear();
                section_offset = 0;
            }
            continue;
        }
        if (stripped.empty()) continue;

        // Magic header
        if (starts_with(stripped, "[brent's_relocatable_format]")) {
            in_table = false;
            continue;
        }

        // Section header like [START OF OBJ_TYPE] -- comment-only, ignore
        if (stripped[0] == '[') continue;

        // Pointer target declaration: ":name"
        if (stripped[0] == ':') {
            // Flush previous table
            if (in_table) {
                doc.tables.push_back({ current_table_name, current_table_strings });
            }
            current_table_name    = stripped.substr(1);
            current_table_strings = {};
            in_table              = true;
            continue;
        }

        // "end" keyword -- end of file marker
        if (stripped == "end") {
            if (in_table) {
                doc.tables.push_back({ current_table_name, current_table_strings });
                in_table = false;
            }
            continue;
        }

        if (in_table) {
            // Inside a pointer table: expect "string \"...\""
            if (starts_with(stripped, "string ")) {
                current_table_strings.push_back(unquote(stripped.substr(7)));
            }
            continue;
        }

        // Data field: <type> <value> [; comment]
        auto [type, rest] = split_first(stripped);

        // Strip inline comment -- but KEEP it: the file names `utilProc` this way, and a
        // name the file gives us is a fact, unlike a name imposed from outside.
        std::string inline_comment;
        size_t semi = rest.find(';');
        if (semi != std::string::npos) {
            inline_comment = rest.substr(semi + 1);
            size_t b = inline_comment.find_first_not_of(" \t");
            inline_comment = (b == std::string::npos) ? "" : inline_comment.substr(b);
            while (!inline_comment.empty() &&
                   (inline_comment.back() == ' ' || inline_comment.back() == '\t'))
                inline_comment.pop_back();
            rest = rest.substr(0, semi);
        }
        // Trim trailing whitespace
        while (!rest.empty() && (rest.back() == ' ' || rest.back() == '\t')) rest.pop_back();

        if (type == "byte" || type == "word" || type == "dword" ||
            type == "ptr"  || type == "symbol" || type == "string") {
            BrfField f;
            f.type    = type;
            f.value   = (type == "string") ? unquote(rest) : rest;
            f.section = current_section;
            f.offset  = section_offset;
            f.comment = inline_comment;
            section_offset += brf_type_size(type);
            doc.fields.push_back(f);
        }
        // Other tokens (like section separators) are silently skipped
    }

    // Flush last table if file didn't have explicit "end"
    if (in_table) {
        doc.tables.push_back({ current_table_name, current_table_strings });
    }

    return doc;
}

// ---- table lookup ----------------------------------------------------

const BrfTable* BrfDoc::find_table(const std::string& name) const {
    for (auto& t : tables)
        if (t.name == name) return &t;
    return nullptr;
}

// ---- serialize -------------------------------------------------------

std::vector<uint8_t> brf_serialize(const BrfDoc& doc) {
    std::vector<uint8_t> out;
    for (auto& line : doc.raw_lines) {
        for (char c : line) out.push_back((uint8_t)c);
        out.push_back('\r');
        out.push_back('\n');
    }
    return out;
}

// ---- value parsing ---------------------------------------------------

int64_t brf_parse_int(const std::string& value) {
    if (value.empty()) return 0;
    // Negative: ^X means -X
    if (value[0] == '^') return -brf_parse_int(value.substr(1));
    // Hex: $XXXX
    if (value[0] == '$') return (int64_t)std::strtoll(value.c_str() + 1, nullptr, 16);
    // Decimal (may be signed)
    return std::strtoll(value.c_str(), nullptr, 10);
}

} // namespace fx
