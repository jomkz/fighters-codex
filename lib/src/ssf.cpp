#include "fx/ssf.h"

namespace fx {

static bool keyword_char(char c) {
    return (c >= 'A' && c <= 'Z') || c == '_';
}

SsfDoc ssf_read(const uint8_t* data, size_t size) {
    SsfDoc doc;
    doc.text = txt_read(data, size);

    for (size_t li = 0; li < doc.text.lines.size(); li++) {
        const std::string& s = doc.text.lines[li].raw;
        size_t p = 0;
        while (p < s.size() && (s[p] == ' ' || s[p] == '\t')) p++;
        if (p >= s.size() || s[p] == '#') continue;

        size_t kw = p;
        while (p < s.size() && keyword_char(s[p])) p++;
        if (p == kw) continue;  // not a keyword line
        SsfStatement st;
        st.line = li;
        st.keyword = s.substr(kw, p - kw);

        // Arguments: comma-separated, quoted strings or bare tokens.
        while (p < s.size()) {
            while (p < s.size() &&
                   (s[p] == ' ' || s[p] == '\t' || s[p] == ',')) p++;
            if (p >= s.size() || s[p] == '#') break;
            std::string arg;
            if (s[p] == '"') {
                p++;
                while (p < s.size() && s[p] != '"') arg += s[p++];
                if (p < s.size()) p++;  // closing quote
            } else {
                while (p < s.size() && s[p] != ',' && s[p] != ' ' &&
                       s[p] != '\t')
                    arg += s[p++];
            }
            st.args.push_back(std::move(arg));
        }
        doc.statements.push_back(std::move(st));
    }
    return doc;
}

std::vector<uint8_t> ssf_write(const SsfDoc& doc) {
    return txt_write(doc.text);
}

} // namespace fx
