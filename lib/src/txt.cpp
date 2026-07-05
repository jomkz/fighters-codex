#include "fx/txt.h"

namespace fx {

TxtDoc txt_read(const uint8_t* data, size_t size) {
    TxtDoc doc;
    size_t start = 0;
    for (size_t i = 0; i <= size; i++) {
        bool at_end = (i == size);
        if (!at_end && data[i] != '\n') continue;
        if (at_end && i == start) break; // no unterminated trailing line

        TxtLine line;
        size_t end = i;
        if (!at_end) {
            line.terminated = true;
            if (end > start && data[end - 1] == '\r') {
                line.crlf = true;
                end--;
            }
        } else {
            line.terminated = false;
        }
        line.raw.assign((const char*)data + start, end - start);

        // Directive tokens: whitespace-separated words starting with '.'
        // ('..' marks the closing form, e.g. "..button").
        size_t p = 0;
        const std::string& s = line.raw;
        while (p < s.size()) {
            while (p < s.size() && (s[p] == ' ' || s[p] == '\t')) p++;
            size_t w = p;
            while (w < s.size() && s[w] != ' ' && s[w] != '\t') w++;
            if (w > p && s[p] == '.')
                line.directives.push_back(s.substr(p, w - p));
            p = w;
        }

        doc.lines.push_back(std::move(line));
        start = i + 1;
    }
    return doc;
}

std::vector<uint8_t> txt_write(const TxtDoc& doc) {
    std::vector<uint8_t> out;
    for (const TxtLine& line : doc.lines) {
        out.insert(out.end(), line.raw.begin(), line.raw.end());
        if (line.terminated) {
            if (line.crlf) out.push_back('\r');
            out.push_back('\n');
        }
    }
    return out;
}

TxtKind txt_classify(const TxtDoc& doc) {
    bool any_directive = false;
    for (const TxtLine& line : doc.lines) {
        for (const std::string& d : line.directives) {
            any_directive = true;
            if (d == ".page" || d == ".button" || d == ".picture")
                return TxtKind::UiTemplate;
        }
    }
    return any_directive ? TxtKind::CampaignDescription : TxtKind::PlainText;
}

size_t txt_count(const TxtDoc& doc, const std::string& directive) {
    size_t n = 0;
    for (const TxtLine& line : doc.lines)
        for (const std::string& d : line.directives)
            if (d == directive) n++;
    return n;
}

} // namespace fx
