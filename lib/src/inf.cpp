#include "fx/inf.h"
#include <cctype>
#include <cstring>

namespace fx {

// Extract a stats key/value from one content line (terminator stripped).
//   Format A: "LENGTH (m): 16.26"   (colon-space separator)
//   Format B: "LENGTH (m) 16.26"    (space separator, no colon)
// Both: no leading whitespace, key contains a unit in parens.
static void try_stat_line(const std::string& line,
                          std::map<std::string, std::string>& stats) {
    if (line.empty() || line[0] == ' ') return;

    auto colon = line.find(": ");
    if (colon != std::string::npos) {
        const std::string key = line.substr(0, colon);
        const std::string val = line.substr(colon + 2);
        if (key.find('(') != std::string::npos && key.find(')') != std::string::npos)
            stats[key] = val;
        return;
    }
    auto rparen = line.rfind(')');
    if (rparen != std::string::npos && rparen + 2 < line.size() &&
        line[rparen + 1] == ' ' && std::isdigit((unsigned char)line[rparen + 2])) {
        const std::string key = line.substr(0, rparen + 1);
        const std::string val = line.substr(rparen + 2);
        if (key.find('(') != std::string::npos)
            stats[key] = val;
    }
}

InfFile inf_parse(const uint8_t* data, size_t size) {
    InfFile result{};
    if (!data || size == 0) return result;

    InfSection cur;

    auto flush = [&]() {
        auto& t = cur.text;
        while (!t.empty() && (t.back() == '\n' || t.back() == '\r' || t.back() == ' '))
            t.pop_back();
        if (!cur.raw.empty())
            result.sections.push_back(cur);
        cur = {};
    };

    // Walk lines keeping terminators, so raw accumulates the file verbatim.
    size_t i = 0;
    while (i < size) {
        size_t start = i;
        while (i < size && data[i] != '\n') ++i;
        size_t content_end = i;                       // exclusive, before '\n'
        if (i < size) ++i;                            // consume '\n'
        if (content_end > start && data[content_end - 1] == '\r')
            --content_end;

        std::string content((const char*)data + start, content_end - start);
        std::string with_term((const char*)data + start, i - start);

        if (!content.empty() && content[0] == '.') {
            flush();
            cur.raw       = with_term;
            cur.directive = content;
            continue;
        }

        try_stat_line(content, result.stats);
        cur.raw  += with_term;
        cur.text += content;
        cur.text += '\n';
    }
    flush();

    result.valid = true;
    return result;
}

std::vector<uint8_t> inf_serialize(const InfFile& inf) {
    std::vector<uint8_t> out;
    for (const auto& s : inf.sections)
        out.insert(out.end(), s.raw.begin(), s.raw.end());
    return out;
}

void inf_rebuild_section(InfSection& s) {
    // Corpus convention: sections end with a blank line before the next
    // directive. Preserve what the old raw did; new sections default to it.
    auto ends_with = [&](const char* suf) {
        size_t n = strlen(suf);
        return s.raw.size() >= n &&
               s.raw.compare(s.raw.size() - n, n, suf) == 0;
    };
    bool blank_tail = s.raw.empty() || ends_with("\r\n\r\n") || ends_with("\n\n");

    std::string out;
    if (!s.directive.empty()) {
        out += s.directive;
        out += "\r\n";
    }
    if (!s.text.empty()) {
        size_t pos = 0;
        while (pos <= s.text.size()) {
            size_t nl = s.text.find('\n', pos);
            if (nl == std::string::npos) {
                out += s.text.substr(pos);
                out += "\r\n";
                break;
            }
            out += s.text.substr(pos, nl - pos);
            out += "\r\n";
            pos = nl + 1;
            if (pos == s.text.size()) break; // text ended exactly at '\n'
        }
    }
    if (blank_tail) out += "\r\n";
    s.raw = std::move(out);
}

} // namespace fx
